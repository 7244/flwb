#pragma once

#include <cstdint>
#include <new>

// free list with blocks
template<
  typename data_t,
  uintptr_t t_max_threads,
  uintptr_t t_data_amount,
  uintptr_t t_block_total_divisor
>
struct flwb_t{
  static constexpr auto _data_amount0 =
    (
      t_data_amount % (t_max_threads * t_block_total_divisor) ?
        (t_max_threads * t_block_total_divisor) -
        (t_data_amount % (t_max_threads * t_block_total_divisor))
      :
        0
    ) +
    t_data_amount
  ;
  static constexpr auto data_per_block = _data_amount0 / (t_max_threads * t_block_total_divisor);
  static constexpr auto data_amount = _data_amount0 + data_per_block * t_max_threads;

  static auto elem_count(auto& a){
    return sizeof(a) / sizeof(a[0]);
  }
  static auto& at_wrap(auto& a, auto& v){
    return a[v % elem_count(a)];
  }

  struct alignas(std::hardware_destructive_interference_size){
    uint32_t current_block = 0;
    uint32_t block_size = data_per_block;

    uint32_t block_index[2];
  }thread_data[t_max_threads];

  template <typename T, uintptr_t t_size>
  struct ring_mpmc_t{
    static constexpr auto invalid = (T)-1;
    static constexpr auto size = t_size;

    alignas(std::hardware_destructive_interference_size) uint64_t producer;
    alignas(std::hardware_destructive_interference_size) uint64_t consumer = 0;

    struct cold_data_t{
      T data[size];
    };

    void produce_unsafe(cold_data_t& cold_data, const T& elem){
      auto p = __atomic_fetch_add(&producer, 1, __ATOMIC_SEQ_CST);

      while(1){
        T expected = invalid;
        if(
          __builtin_expect(
            __atomic_compare_exchange_n(
              &at_wrap(cold_data.data, p),
              &expected,
              elem,
              0,
              __ATOMIC_SEQ_CST,
              __ATOMIC_SEQ_CST
            ),
            true
          )
        ){
          break;
        }
        // its almost impossible to come here. so no relax needed.
      }
    }
    T consume_unsafe(cold_data_t& cold_data){
      auto c = __atomic_fetch_add(&consumer, 1, __ATOMIC_SEQ_CST);

      while(1){
        auto d = __atomic_exchange_n(&at_wrap(cold_data.data, c), invalid, __ATOMIC_SEQ_CST);
        if(__builtin_expect(d != invalid, true)){
          return d;
        }
        // its almost impossible to come here. so no relax needed.
      }
    }
  };

  ring_mpmc_t<uint32_t, data_amount / data_per_block> ring_full;
  ring_mpmc_t<uint32_t, data_amount / data_per_block + t_max_threads * 2> ring_free;

  decltype(ring_full)::cold_data_t ring_full_cold_data;
  decltype(ring_free)::cold_data_t ring_free_cold_data;

  uint32_t blocks[data_amount / data_per_block + t_max_threads * 2][data_per_block];

  uint32_t consume_unsafe(uintptr_t thread_index){
    auto& td = thread_data[thread_index];
    while(td.block_size == 0){
      td.block_size = data_per_block;
      if(td.current_block != 0){
        td.current_block -= 1;
        break;
      }
      ring_free.produce_unsafe(ring_free_cold_data, td.block_index[td.current_block]);
      td.block_index[td.current_block] = ring_full.consume_unsafe(ring_full_cold_data);
    }

    td.block_size -= 1;

    return blocks[td.block_index[td.current_block]][td.block_size];
  }
  void produce_unsafe(uintptr_t thread_index, uint32_t data_index){
    auto& td = thread_data[thread_index];
    while(td.block_size == data_per_block){
      td.block_size = 0;
      if(td.current_block == 0){
        td.current_block += 1;
        break;
      }
      ring_full.produce_unsafe(ring_full_cold_data, td.block_index[td.current_block]);
      td.block_index[td.current_block] = ring_free.consume_unsafe(ring_free_cold_data);
    }

    blocks[td.block_index[td.current_block]][td.block_size] = data_index;

    td.block_size += 1;
  }

  flwb_t(){
    ring_full.producer = decltype(ring_full)::size;
    for(auto i = ring_full.producer; i--;){
      ring_full_cold_data.data[i] = i;
      for(auto bi = data_per_block; bi--;){
        blocks[i][bi] = i * data_per_block + bi;
      }
    }

    ring_free.producer = elem_count(blocks) - decltype(ring_full)::size;
    for(auto i = ring_free.producer; i--;){
      ring_free_cold_data.data[i] = decltype(ring_full)::size + i;
    }
    for(auto i = decltype(ring_free)::size - ring_free.producer; i--;){
      ring_free_cold_data.data[ring_free.producer + i] = decltype(ring_free)::invalid;
    }

    for(auto i = t_max_threads; i--;){
      thread_data[i].block_index[0] = ring_full.consume_unsafe(ring_full_cold_data);
      thread_data[i].block_index[1] = ring_free.consume_unsafe(ring_free_cold_data);
    }
  }

  data_t datas[data_amount];
};
