#include "../PIDX_io.h"

static int intersectNDChunk(Ndim_patch A, Ndim_patch B);


PIDX_return_code partition_setup(PIDX_hybrid_idx_io file, int gi, int svi)
{
  int *colors;
  int index_i = 0, index_j = 0, index_k = 0;
  int i = 0, j = 0, k = 0, d = 0;
  //PIDX_return_code ret;

  int rank = 0;
  MPI_Comm_rank(file->global_comm, &rank);

  colors = malloc(sizeof(*colors) * file->idx_d->partition_count[0] * file->idx_d->partition_count[1] * file->idx_d->partition_count[2]);
  memset(colors, 0, sizeof(*colors) * file->idx_d->partition_count[0] * file->idx_d->partition_count[1] * file->idx_d->partition_count[2]);
  file->idx_d->color = (file->idx_d->partition_count[0] * file->idx_d->partition_count[1] * file->idx_d->partition_count[2]) + 1;

  for (k = 0; k < file->idx_d->partition_count[2]; k++)
    for (j = 0; j < file->idx_d->partition_count[1]; j++)
      for (i = 0; i < file->idx_d->partition_count[0]; i++)
      {
        colors[(file->idx_d->partition_count[0] * file->idx_d->partition_count[1] * k) + (file->idx_d->partition_count[0] * j) + i] = (file->idx_d->partition_count[0] * file->idx_d->partition_count[1] * k) + (file->idx_d->partition_count[0] * j) + i;
      }

  Ndim_patch local_proc_patch = (Ndim_patch)malloc(sizeof (*local_proc_patch));
  memset(local_proc_patch, 0, sizeof (*local_proc_patch));

  PIDX_variable_group var_grp = file->idx->variable_grp[gi];
  PIDX_variable var = var_grp->variable[svi];

  if (var->patch_group_count == 1)
  {
    for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
    {
      local_proc_patch->offset[d] = var->rst_patch_group[0]->reg_patch->offset[d];
      local_proc_patch->size[d] = var->rst_patch_group[0]->reg_patch->size[d];
    }
    Ndim_patch reg_patch = (Ndim_patch)malloc(sizeof (*reg_patch));
    memset(reg_patch, 0, sizeof (*reg_patch));

    PointND bounds_point;
    int maxH = 0;
    bounds_point.x = (int) file->idx_d->partition_count[0];
    bounds_point.y = (int) file->idx_d->partition_count[1];
    bounds_point.z = (int) file->idx_d->partition_count[2];
    char bitSequence[512];
    char bitPattern[512];
    GuessBitmaskPattern(bitSequence, bounds_point);
    maxH = strlen(bitSequence);

    for (i = 0; i <= maxH; i++)
      bitPattern[i] = RegExBitmaskBit(bitSequence, i);

    int z_order = 0;
    int number_levels = maxH - 1;

    for (i = 0, index_i = 0; i < file->idx->bounds[0]; i = i + file->idx_d->partition_size[0], index_i++)
    {
      for (j = 0, index_j = 0; j < file->idx->bounds[1]; j = j + file->idx_d->partition_size[1], index_j++)
      {
        for (k = 0, index_k = 0; k < file->idx->bounds[2]; k = k + file->idx_d->partition_size[2], index_k++)
        {
          reg_patch->offset[0] = i;
          reg_patch->offset[1] = j;
          reg_patch->offset[2] = k;
          reg_patch->size[0] = file->idx_d->partition_size[0];
          reg_patch->size[1] = file->idx_d->partition_size[1];
          reg_patch->size[2] = file->idx_d->partition_size[2];

         if (intersectNDChunk(reg_patch, local_proc_patch))
          {
            PointND xyzuv_Index;
            xyzuv_Index.x = index_i;
            xyzuv_Index.y = index_j;
            xyzuv_Index.z = index_k;

            z_order = 0;
            PointND zero;
            zero.x = 0;
            zero.y = 0;
            zero.z = 0;
            memset(&zero, 0, sizeof (PointND));

            int cnt = 0;
            for (cnt = 0; memcmp(&xyzuv_Index, &zero, sizeof (PointND)); cnt++, number_levels--)
            {
              int bit = bitPattern[number_levels];
              z_order |= ((unsigned long long) PGET(xyzuv_Index, bit) & 1) << cnt;
              PGET(xyzuv_Index, bit) >>= 1;
            }

            file->idx_d->color = colors[z_order];
            //printf("[%d] ---> %d\n", rank, file->idx_d->color);

            assert(var->sim_patch_count == 1);
            //var->sim_patch_count = 1;
            var->sim_patch[0]->offset[0] = var->rst_patch_group[0]->reg_patch->offset[0];
            var->sim_patch[0]->offset[1] = var->rst_patch_group[0]->reg_patch->offset[1];
            var->sim_patch[0]->offset[2] = var->rst_patch_group[0]->reg_patch->offset[2];

            var->sim_patch[0]->size[0] = var->rst_patch_group[0]->reg_patch->size[0];
            var->sim_patch[0]->size[1] = var->rst_patch_group[0]->reg_patch->size[1];
            var->sim_patch[0]->size[2] = var->rst_patch_group[0]->reg_patch->size[2];

            break;
          }
        }
      }
    }
    free(reg_patch);
  }
  else
  {
    file->idx->bounds[0] = 0;//reg_patch->size[0];
    file->idx->bounds[1] = 0;//reg_patch->size[1];
    file->idx->bounds[2] = 0;//reg_patch->size[2];

    var->sim_patch_count = 0;
    var->sim_patch[0]->offset[0] = 0;
    var->sim_patch[0]->offset[1] = 0;
    var->sim_patch[0]->offset[2] = 0;

    var->sim_patch[0]->size[0] = 0;//-1;
    var->sim_patch[0]->size[1] = 0;//-1;
    var->sim_patch[0]->size[2] = 0;//-1;
  }


  free(colors);

  //
  char file_name_skeleton[1024];
  strncpy(file_name_skeleton, file->idx->filename, strlen(file->idx->filename) - 4);
  file_name_skeleton[strlen(file->idx->filename) - 4] = '\0';

  if (file->idx_d->partition_count[0] != 1 || file->idx_d->partition_count[1] != 1 || file->idx_d->partition_count[2] != 1)
  {
    sprintf(file->idx->filename_partition, "%s_%d.idx", file_name_skeleton, file->idx_d->color);
    sprintf(file->idx->filename_file_zero, "%s_%s.idx", file_name_skeleton, "file_zero");
  }
  else
  {
    strcpy(file->idx->filename_partition, file->idx->filename);
    strcpy(file->idx->filename_file_zero, file->idx->filename);
  }

  strcpy(file->idx->filename_global, file->idx->filename);

  free(local_proc_patch);
  local_proc_patch = 0;

  return PIDX_success;
}


PIDX_return_code create_local_comm(PIDX_hybrid_idx_io file, int gi)
{
  int rank = 0;
  int gnprocs = 1;
  int ret;

  PIDX_variable_group var_grp = file->idx->variable_grp[gi];

  ret = MPI_Comm_split(file->global_comm, file->idx_d->color, rank, &(file->comm));
  if (ret != MPI_SUCCESS)
  {
    fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  MPI_Comm_rank(file->comm,  &rank);
  MPI_Comm_size(file->global_comm, &gnprocs);

  memset(var_grp->rank_buffer, 0, gnprocs * sizeof(*var_grp->rank_buffer));
  MPI_Allgather(&rank, 1, MPI_INT, var_grp->rank_buffer, 1, MPI_INT, file->global_comm);

  return PIDX_success;
}


PIDX_return_code destroy_local_comm(PIDX_hybrid_idx_io file)
{
  int ret;
  ret = MPI_Comm_free(&(file->comm));
  if (ret != MPI_SUCCESS)
  {
    fprintf(stdout,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }
  return PIDX_success;
}


PIDX_return_code find_partition_count(PIDX_hybrid_idx_io file)
{
  int d = 0;
  idx_dataset_derived_metadata idx = file->idx_d;

  // calculate number of partitions
  for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
  {
      file->idx_d->partition_count[d] = file->idx->bounds[d] / file->idx_d->partition_size[d];
      if (file->idx->bounds[d] % file->idx_d->partition_size[d] != 0)
          file->idx_d->partition_count[d]++;

      file->idx_d->partition_count[d] = pow(2, (int)ceil(log2(file->idx_d->partition_count[d])));
  }

  idx->shared_block_level = (int)log2(idx->partition_count[0] * idx->partition_count[1] * idx->partition_count[2]) + file->idx->bits_per_block + 1;
  if (idx->shared_block_level >= idx->maxh)
    idx->shared_block_level = idx->maxh;

  int partion_level = (int) log2(idx->partition_count[0] * idx->partition_count[1] * idx->partition_count[2]);
  idx->total_partiton_level = file->idx->bits_per_block + (int)log2(file->idx->blocks_per_file) + 1 + partion_level;
  if (idx->total_partiton_level >= idx->maxh)
    idx->total_partiton_level = idx->maxh;

  return PIDX_success;
}


static int intersectNDChunk(Ndim_patch A, Ndim_patch B)
{
  int d = 0, check_bit = 0;
  for (d = 0; d < PIDX_MAX_DIMENSIONS; d++)
    check_bit = check_bit || (A->offset[d] + A->size[d] - 1) < B->offset[d] || (B->offset[d] + B->size[d] - 1) < A->offset[d];

  return !(check_bit);
}