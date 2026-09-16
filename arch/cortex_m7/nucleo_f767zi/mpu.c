#include "port.h"

// Were gonna default it to No Access
// For the handling between during context switch, i think during the mpu switch
// we have to enter critical If a higher prio hits while inside in the interrupt
// it might break

/* Pseudo Code
  after stack pointers swap
  portentercrit
  disable mpu
  use task fields for the region data for the swap that ill make soon
  portexitcrit
*/

// This will be inside task_create_static
void port_mpu_configuration(task_handle_t this_task, size_t capacity) {}
