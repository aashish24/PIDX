/*****************************************************
 **  PIDX Parallel I/O Library                      **
 **  Copyright (c) 2010-2014 University of Utah     **
 **  Scientific Computing and Imaging Institute     **
 **  72 S Central Campus Drive, Room 3750           **
 **  Salt Lake City, UT 84112                       **
 **                                                 **
 **  PIDX is licensed under the Creative Commons    **
 **  Attribution-NonCommercial-NoDerivatives 4.0    **
 **  International License. See LICENSE.md.         **
 **                                                 **
 **  For information about this project see:        **
 **  http://www.cedmav.com/pidx                     **
 **  or contact: pascucci@sci.utah.edu              **
 **  For support: PIDX-support@visus.net            **
 **                                                 **
 *****************************************************/


#include "../../PIDX_inc.h"


PIDX_return_code HELPER_Hz_encode(PIDX_hz_encode_id id)
{
  PIDX_variable_group var_grp = id->idx->variable_grp[id->group_index];
  int i = 0, k = 0, b = 0, v = 0;
  unsigned long long global_hz, element_count = 0, lost_element_count = 0;
  unsigned long long ZYX[PIDX_MAX_DIMENSIONS];
  int check_bit = 1, s = 0;
  double dvalue_1 = 0, dvalue_2 = 0;
  float fvalue_1 = 0, fvalue_2 = 0;
  unsigned long long uvalue_1 = 0, uvalue_2 = 0;

  for(v = id->first_index; v <= id->last_index; v++)
  {
    PIDX_variable var = var_grp->variable[v];
    //if (file->idx_c->lrank == 0)
    //  fprintf(stderr, "[%d %d] var->type_name = %s PC = %d\n", v, (id->last_index - id->first_index + 1), var->type_name, var->patch_group_count);
    for (b = 0; b < var->patch_group_count; b++)
    {
      for (i = 0; i < id->idx_d->maxh; i++)
      {
        if (var->hz_buffer[b]->nsamples_per_level[i][0] * var->hz_buffer[b]->nsamples_per_level[i][1] * var->hz_buffer[b]->nsamples_per_level[i][2] != 0)
        {
          //if (file->idx_c->lrank == 0)
          //  fprintf(stderr, "[B %d L %d] - %d\n", b, i, (var->hz_buffer[b]->end_hz_index[i] - var->hz_buffer[b]->start_hz_index[i] ));
          for (k = 0; k <= (var->hz_buffer[b]->end_hz_index[i] - var->hz_buffer[b]->start_hz_index[i]) * 1; k++)
          {
            global_hz = var->hz_buffer[b]->start_hz_index[i] + k;
            Hz_to_xyz(id->idx->bitPattern, id->idx_d->maxh - 1, global_hz, ZYX);
            if ((ZYX[0] < id->idx->box_bounds[0] && ZYX[1] < id->idx->box_bounds[1] && ZYX[2] < id->idx->box_bounds[2]))
            {
              check_bit = 1, s = 0;
              if (strcmp(var->type_name, FLOAT64) == 0)
              {
                dvalue_1 = 100 + v + s + (id->idx->bounds[0] * id->idx->bounds[1]*(ZYX[2]))+(id->idx->bounds[0]*(ZYX[1])) + ZYX[0];
                dvalue_2 = *(*((double**)var->hz_buffer[b]->buffer + i) + ((k * var->vps) + s));

                //fprintf(stderr, "X[%lld]  %f %f\n", (long long)global_hz, dvalue_1, dvalue_2);
                check_bit = check_bit && (dvalue_1  == dvalue_2);
              }
              else if (strcmp(var->type_name, FLOAT32) == 0)
              {
                fvalue_1 = 100 + v + s + (id->idx->bounds[0] * id->idx->bounds[1]*(ZYX[2]))+(id->idx->bounds[0]*(ZYX[1])) + ZYX[0];
                fvalue_2 = *(*((float**)var->hz_buffer[b]->buffer + i) + ((k * var->vps) + s));

                //if (file->idx_c->lrank == 0)
                //fprintf(stderr, "%f %f\n", fvalue_1, fvalue_2);
                check_bit = check_bit && (fvalue_1 == fvalue_2);
              }
              else if (strcmp(var->type_name, FLOAT64_RGB) == 0)
              {
                for (s = 0; s < 3; s++)
                {
                  dvalue_1 = v + s + (id->idx->bounds[0] * id->idx->bounds[1]*(ZYX[2]))+(id->idx->bounds[0]*(ZYX[1])) + ZYX[0];
                  memcpy(&dvalue_2, var->hz_buffer[b]->buffer[i] + ((k * 3) + s) * sizeof(double), sizeof(double));
                  //fprintf(stderr, "Y[%d] %f -- %f\n", v, dvalue_1, dvalue_2);

                  check_bit = check_bit && (dvalue_1  == dvalue_2);
                }
              }
              if (strcmp(var->type_name, UINT64) == 0)
              {
                uvalue_1 = v + s + (id->idx->bounds[0] * id->idx->bounds[1]*(ZYX[2]))+(id->idx->bounds[0]*(ZYX[1])) + ZYX[0] + (id->idx_d->color * id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2]);
                uvalue_2 = *(*((unsigned long long**)var->hz_buffer[b]->buffer + i) + ((k * var->vps) + s));
                check_bit = check_bit && (uvalue_1  == uvalue_2);
              }
              else if (strcmp(var->type_name, UINT64_RGB) == 0)
              {
                for (s = 0; s < 3; s++)
                {
                  uvalue_1 = v + s + (id->idx->bounds[0] * id->idx->bounds[1]*(ZYX[2]))+(id->idx->bounds[0]*(ZYX[1])) + ZYX[0] + (id->idx_d->color * id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2]);
                  memcpy(&uvalue_2, var->hz_buffer[b]->buffer[i] + ((k * 3) + s) * sizeof(double), sizeof(double));

                  check_bit = check_bit && (uvalue_1  == uvalue_2);
                }
              }

              if (check_bit == 1 && fvalue_1 != 0 && fvalue_2 != 0)
              {
                //fprintf(stderr, "HZ [%d] %f %f\n", file->idx_c->lrank, dvalue_1, dvalue_2);
                //if (file->idx_c->lrank == 0)
                //fprintf(stderr, "C [level %d] [%d] %lld [HZ] %f %f (%lld :: %lld %lld %lld)\n", i, file->idx_c->lrank, (long long)global_hz, fvalue_1, fvalue_2, (long long)global_hz, (long long)ZYX[0], (long long)ZYX[1], (long long)ZYX[2]);
                element_count++;
              }
              else
              {
                //if (file->idx_c->lrank == 0)
                  //fprintf(stderr, "W [level %d] [%d] %lld [HZ] %f %f (%lld :: %lld %lld %lld)\n", i, file->idx_c->lrank, (long long)global_hz, fvalue_1, fvalue_2, (long long)global_hz, (long long)ZYX[0], (long long)ZYX[1], (long long)ZYX[2]);
                //fprintf(stderr, "%lld [HZ] %f %f (%lld :: %lld %lld %lld)\n", (long long)global_hz, fvalue_1, fvalue_2, (long long)global_hz, (long long)ZYX[0], (long long)ZYX[1], (long long)ZYX[2]);
                lost_element_count++;
              }

            }
          }
        }
      }
    }
  }

#if PIDX_HAVE_MPI
  unsigned long long global_volume = 0;
  if (id->idx_d->parallel_mode == 1)
    MPI_Allreduce(&element_count, &global_volume, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, id->idx_c->local_comm);
  else
    global_volume = element_count;

  //fprintf(stderr, "[HZ] Volume [%lld] and Volume [%lld]\n", global_volume, (unsigned long long)(id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2] * (id->last_index - id->first_index + 1)));

  if (global_volume != (unsigned long long) id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2] * (id->last_index - id->first_index + 1))
  {
    if (id->idx_c->lrank == 0)
      fprintf(stderr, "[HZ Debug FAILED!!!!] [Color %d] [Recorded Volume %lld] [Actual Volume %lld]\n", id->idx_d->color, (long long) global_volume, (long long) id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2] * (id->last_index - id->first_index + 1));

    if (id->idx_c->lrank == 0)
      fprintf(stderr, "[HZ]  file->idx_c->lrank %d Color %d [LOST ELEMENT COUNT %lld] [FOUND ELEMENT COUNT %lld] [TOTAL ELEMNTS %lld] \n", id->idx_c->lrank,  id->idx_d->color, (long long) lost_element_count, (long long) element_count, (long long) (id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2]) * (id->last_index - id->first_index + 1));

    return PIDX_err_hz;
  }

  else
  {
    if (id->idx_c->lrank == 0)
      fprintf(stderr, "[HZ Debug PASSED!!!!]  [Color %d] [Recorded Volume %lld] [Actual Volume %lld]\n", id->idx_d->color, (long long) global_volume, (long long) id->idx->bounds[0] * id->idx->bounds[1] * id->idx->bounds[2] * (id->last_index - id->first_index + 1));
  }
#endif
  return PIDX_success;
}
