#include <WITCH/WITCH.h>
#include <WITCH/generic_alloc.h>

#include "flwb.h"

struct pile_t{
  flwb_t<uint64_t, 32, 1000000, 4> flwb;
};
pile_t *pile;

int main(){
  pile = (pile_t *)__generic_mmap(sizeof(pile_t));
  if((uintptr_t)pile > (uintptr_t)-0x1000){
    __abort();
  }
  new (pile) pile_t;

  return 0;
}
