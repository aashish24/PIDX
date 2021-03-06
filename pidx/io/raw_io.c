#include "../PIDX_inc.h"

static PIDX_return_code group_meta_data_init(PIDX_io file, int gi, int svi, int evi, int mode);
static PIDX_return_code dump_process_extent(PIDX_io file);

/// Raw Write Steps
/********************************************************
*  Step 0: Setup Group related meta-data                *
*                                                       *
*  Step 1: Setup Restructuring Phase                    *
*  Step 2: Perform data Restructuring                   *
*  Step 3: Perform actual file IO                       *
*  Step 4: cleanup for Steps 1                          *
*********************************************************/

PIDX_return_code PIDX_raw_write(PIDX_io file, int gi, int svi, int evi)
{  
  int si = 0, ei = 0;
  PIDX_return_code ret;

  // Step 0
  ret = group_meta_data_init(file, gi, svi, evi, PIDX_WRITE);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  ret = dump_process_extent(file);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  file->idx->variable_pipe_length = file->idx->variable_count;
  for (si = svi; si < evi; si = si + (file->idx->variable_pipe_length + 1))
  {
    ei = ((si + file->idx->variable_pipe_length) >= (evi)) ? (evi - 1) : (si + file->idx->variable_pipe_length);
    file->idx->variable_grp[gi]->variable_tracker[si] = 1;

    // Step 1: Setup restructuring buffers
    ret = restructure_setup(file, gi, si, ei, PIDX_WRITE);
    if (ret != PIDX_success)
    {
      fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }

    // Step 2: Perform data restructuring
    ret = restructure(file, PIDX_WRITE);
    if (ret != PIDX_success)
    {
      fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }

    // Step 3: Write out restructured data
    ret = restructure_io(file, PIDX_WRITE);
    if (ret != PIDX_success)
    {
      fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }

    // Step 4: Cleanup all buffers and ids
    ret = restructure_cleanup(file);
    if (ret != PIDX_success)
    {
      fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }
  }

  return PIDX_success;
}



/// Raw Read Steps
/********************************************************
*  Step 0: Setup Group related meta-data                *
*                                                       *
*  Step 1: Setup Restructuring Phase                    *
*  Step 2: Perform actual file IO                       *
*  Step 3: Perform data Restructuring                   *
*  Step 4: cleanup for Steps 1                          *
*********************************************************/

PIDX_return_code PIDX_raw_read(PIDX_io file, int gi, int svi, int evi)
{
  int si = 0, ei = 0;
  PIDX_return_code ret;

  // Step 0
  ret = group_meta_data_init(file, gi, svi, evi, PIDX_READ);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  int rst_case_type = 0;
  PIDX_variable_group var_grp = file->idx->variable_grp[gi];
  PIDX_variable var0 = var_grp->variable[svi];
  int patch_count = var0->sim_patch_count;
  int max_patch_count = 0;

  MPI_Allreduce(&patch_count, &max_patch_count, 1, MPI_INT, MPI_MAX, file->idx_c->global_comm);
  if (max_patch_count > 1)
    rst_case_type = 1;

  file->idx->variable_pipe_length = file->idx->variable_count;
  if (file->idx_d->data_core_count == file->idx_c->gnprocs && rst_case_type == 0)
  {
    for (si = svi; si < evi; si = si + (file->idx->variable_pipe_length + 1))
    {
      ei = ((si + file->idx->variable_pipe_length) >= (evi)) ? (evi - 1) : (si + file->idx->variable_pipe_length);
      file->idx->variable_grp[gi]->variable_tracker[si] = 1;

      // Step 1: Setup restructuring buffers
      ret = restructure_setup(file, gi, si, ei, PIDX_READ);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }

      // Step 2: Write out restructured data
      ret = restructure_io(file, PIDX_READ);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }

      // Step 3: Perform data restructuring
      ret = restructure(file, PIDX_READ);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }

      // Step 4: Cleanup all buffers and ids
      ret = restructure_cleanup(file);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }
    }
  }
  else
  {
    for (si = svi; si < evi; si = si + (file->idx->variable_pipe_length + 1))
    {
      ei = ((si + file->idx->variable_pipe_length) >= (evi)) ? (evi - 1) : (si + file->idx->variable_pipe_length);
      file->idx->variable_grp[gi]->variable_tracker[si] = 1;

      ret = restructure_forced_read(file, si, ei);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_file;
      }
    }
  }

  return PIDX_success;
}


static PIDX_return_code group_meta_data_init(PIDX_io file, int gi, int svi, int evi, int mode)
{
  int ret;
  PIDX_time time = file->idx_d->time;

  time->set_reg_box_start = MPI_Wtime();
  ret = set_rst_box_size(file, gi, svi);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }
  time->set_reg_box_end = MPI_Wtime();

  if (mode == PIDX_WRITE)
  {
    time->header_io_start = PIDX_get_time();
    // Creates the file heirarchy and writes the header info for all binary files
    ret = init_raw_headers_layout(file, gi, svi, evi, file->idx->filename);
    if (ret != PIDX_success)
    {
      fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }
    time->header_io_end = PIDX_get_time();
  }

  return PIDX_success;
}


static PIDX_return_code dump_process_extent(PIDX_io file)
{
  int i, j, k;
  for (i = 0; i < file->idx->variable_group_count; i++)
  {
    PIDX_variable_group var_grp = file->idx->variable_grp[i];
    for (j = 0; j < var_grp->variable_count; j++)
    {
      for (k = 0; k < var_grp->variable[j]->sim_patch_count; k++)
      {
        if (file->idx_dbg->state_dump == PIDX_META_DATA_DUMP_ONLY || file->idx_dbg->state_dump == PIDX_NO_IO_AND_META_DATA_DUMP)
        {
          fprintf(file->idx_dbg->local_dump_fp, "[%d] [%d] %d %d %d %d %d %d\n", j, k, (int)var_grp->variable[j]->sim_patch[k]->offset[0], (int)var_grp->variable[j]->sim_patch[k]->offset[1], (int)var_grp->variable[j]->sim_patch[k]->offset[2], (int)var_grp->variable[j]->sim_patch[k]->size[0], (int)var_grp->variable[j]->sim_patch[k]->size[1], (int)var_grp->variable[j]->sim_patch[k]->size[2]);
          fflush(file->idx_dbg->local_dump_fp);
        }
      }
      if (file->idx_dbg->state_dump == PIDX_META_DATA_DUMP_ONLY || file->idx_dbg->state_dump == PIDX_NO_IO_AND_META_DATA_DUMP)
      {
        fprintf(file->idx_dbg->local_dump_fp, "\n");
        fflush(file->idx_dbg->local_dump_fp);
      }
    }
  }

  return PIDX_success;
}
