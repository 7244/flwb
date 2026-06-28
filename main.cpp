#include <WITCH/WITCH.h>
#include <WITCH/generic_alloc.h>

#include "flwb.h"

#include <thread>
#include <random>
#include <print>

#define MAX_THREADS (32)
#define MAX_ELEMENTS (1000000)

struct pile_t{
  flwb_t<uint64_t, MAX_THREADS, MAX_ELEMENTS, 4> flwb;
  struct alignas(std::hardware_destructive_interference_size){
    uint64_t sum;
  }thread_data[MAX_THREADS];
};
pile_t *pile;

void worker(uint32_t thread_index){
  constexpr auto hard_limit = MAX_ELEMENTS / MAX_THREADS;
  auto elems = (uint32_t *)__generic_mmap(hard_limit * sizeof(uint32_t));
  if((uintptr_t)elems > (uintptr_t)-0x1000){
    __abort();
  }

  std::mt19937 gen(std::random_device{}());
  
  while(1){
    auto limit = std::uniform_int_distribution<decltype(hard_limit)>(0, hard_limit - 1)(gen);

    for(auto i = limit; i--;){
      elems[i] = pile->flwb.consume_unsafe(thread_index);
    }
    for(auto i = limit; i--;){
      pile->flwb.produce_unsafe(thread_index, elems[i]);
    }

    __atomic_add_fetch(&pile->thread_data[thread_index].sum, limit, __ATOMIC_SEQ_CST);
  }
}

int main(){
  pile = (pile_t *)__generic_mmap(sizeof(pile_t));
  if((uintptr_t)pile > (uintptr_t)-0x1000){
    __abort();
  }
  new (pile) pile_t;

  std::thread threads[MAX_THREADS];
  for(auto i = MAX_THREADS; i--;){
    threads[i] = std::thread(worker, i);
  }

  std::println("data_per_block is: {}", pile->flwb.data_per_block);

  uint64_t sum = 0;
  while(1){
    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t new_sum = 0;
    for(auto i = MAX_THREADS; i--;){
      new_sum += __atomic_load_n(&pile->thread_data[i].sum, __ATOMIC_SEQ_CST);
    }
    std::println("{}", new_sum - sum);
    sum = new_sum;
  }

  return 0;
}
