/*
 * cali-basic-c
 * Caliper C interface test
 */

#include <cali.h>

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum phases { PHASE_MAIN = 0, PHASE_LOOP = 1 };
const char* phase_string[] = { "main", "loop" };


int main(int argc, char* argv[])
{
  cali_id_t attr_phase = cali_create_attribute("phase", CALI_TYPE_STRING, CALI_ATTR_DEFAULT);

  cali_begin(attr_phase, phase_string[PHASE_MAIN], strlen(phase_string[PHASE_MAIN]));

  int count = argc > 1 ? atoi(argv[1]) : 4;

  cali_id_t attr_iter = cali_create_attribute("iter", CALI_TYPE_INT, CALI_ATTR_ASVALUE);

  cali_begin(attr_phase, phase_string[PHASE_LOOP], strlen(phase_string[PHASE_LOOP]));

  for (int64_t i = 0; i < count; ++i) {
    cali_set(attr_iter, &i, sizeof(int64_t));
  }

  cali_end(attr_iter); /* unset "iter" */
  cali_end(attr_phase); /* end "loop" */

  cali_end(attr_phase); /* end "main" */
}
