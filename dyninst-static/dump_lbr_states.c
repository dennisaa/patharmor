#include <stdio.h>

#include "lbr-state.h"


void
dump_human_readable(const char *fname, struct lbr_paths *kstates)
{
  unsigned int i, j, k;

  printf("read state file %s [%u functions, %u address-taken, %u state sets]\n",
         fname, kstates->funcs, kstates->ats, kstates->state_maps);

  printf("+------------------------------+\n");
  printf("|           functions          |\n");
  printf("+------------------------------+\n");
  for(i = 0; i < kstates->funcs; i++) {
    printf("%u: %s @ 0x%jx\n", i, kstates->func[i].fname, kstates->func[i].fptr);
  }

  printf("+------------------------------+\n");
  printf("|         address-taken        |\n");
  printf("+------------------------------+\n");
  for(i = 0; i < kstates->ats; i++) {
    printf("%u: 0x%jx\n", i, kstates->address_taken[i]);
  }

  printf("+------------------------------+\n");
  printf("|          state sets          |\n");
  printf("+------------------------------+\n");
  for(i = 0; i < kstates->state_maps; i++) {
    printf("%u: 0x%jx (%u states)\n", i, kstates->state_map[i].to, kstates->state_map[i].states);
    for(j = 0; j < kstates->state_map[i].states; j++) {
      printf("%u.%u: [", i, j);
      for(k = 0; k < DIGEST_LENGTH; k++) printf("%02x", kstates->state_map[i].state[j].hash[k]);
      printf("] ");
      for(k = 0; k < WINDOW_SIZE; k++) {
        printf("0x%jx -> 0x%jx", 
               kstates->state_map[i].state[j].from[k],
               kstates->state_map[i].state[j].to[k]);
        if(k < WINDOW_SIZE-1) printf(" | ");
      }
      printf("\n");
    }
  }
}


void
dump_python(const char *fname, struct lbr_paths *kstates)
{
  unsigned int i, j, k;

  printf("lbr_states = {}\n");
  printf("lbr_states['path'] = '%s'\n", fname);

  printf("lbr_states['functions'] = []\n");
  for(i = 0; i < kstates->funcs; i++) {
    printf("lbr_states['functions'].append({'fname': '%s', 'fptr': 0x%jx})\n",
           kstates->func[i].fname, kstates->func[i].fptr);
  }

  printf("lbr_states['addrtaken'] = []\n");
  for(i = 0; i < kstates->ats; i++) {
    printf("lbr_states['addrtaken'].append(0x%jx)\n", kstates->address_taken[i]);
  }

  printf("lbr_states['states'] = {}\n");
  for(i = 0; i < kstates->state_maps; i++) {
    printf("lbr_states['states'][0x%jx] = []\n", kstates->state_map[i].to);
    for(j = 0; j < kstates->state_map[i].states; j++) {
      printf("lbr_states['states'][0x%jx].append({'hash': '", kstates->state_map[i].to);
      for(k = 0; k < DIGEST_LENGTH; k++) printf("%02x", kstates->state_map[i].state[j].hash[k]);
      printf("', 'edges': [");
      for(k = 0; k < WINDOW_SIZE; k++) {
        printf("(0x%jx, 0x%jx)", 
               kstates->state_map[i].state[j].from[k],
               kstates->state_map[i].state[j].to[k]);
        if(k < WINDOW_SIZE-1) printf(", ");
      }
      printf("]})\n");
    }
  }
}


int
main(int argc, char *argv[])
{
  const char *fname, *format;
  struct lbr_paths *kstates;

  if(argc < 2) {
    printf("Usage: %s <state-file> [format]\n", argv[0]);
    return 1;
  }

  fname = argv[1];
  kstates = read_paths(fname);
  if(!kstates) {
    fprintf(stderr, "Failed to read %s\n", fname);
    return 1;
  }
  hash_paths(kstates);

  format = "human";
  if(argc > 2) {
    format = argv[2];
  }

  if(!strcmp(format, "human")) {
    dump_human_readable(fname, kstates);
  } else if(!strcmp(format, "python")) {
    dump_python(fname, kstates);
  } else {
    printf("Unrecognized output format: %s\n", format);
    return 1;
  }

  return 0;
}

