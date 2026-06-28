#include <WITCH/WITCH.h>
#include <WITCH/generic_alloc.h>

#include "flwb.h"

#include <thread>

#define MAX_THREADS (8)

struct pile_t{
  flwb_t<uint64_t, MAX_THREADS, 100000000, 4096> flwb;
};
pile_t *pile;

void worker(uint32_t thread_index){
  constexpr auto loop1_limit = 10000000;
  auto elems = (uint32_t *)__generic_mmap(loop1_limit * sizeof(uint32_t));
  if((uintptr_t)elems > (uintptr_t)-0x1000){
    __abort();
  }

  for(auto loop0 = 1; loop0--;){
    for(auto loop1 = loop1_limit; loop1--;){
      elems[loop1] = pile->flwb.consume_unsafe(thread_index);
    }
    for(auto loop1 = loop1_limit; loop1--;){
      pile->flwb.produce_unsafe(thread_index, elems[loop1]);
    }
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
  for(auto i = MAX_THREADS; i--;){
    threads[i].join();
  }

  return 0;
}
