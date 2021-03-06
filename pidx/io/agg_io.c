#include "../PIDX_inc.h"
static int lgi = 0;

static PIDX_return_code PIDX_global_async_io(PIDX_io file, PIDX_file_io_id **io_id,
                                             Agg_buffer **agg_buffer,
                                             PIDX_block_layout* block_layout_by_level,
                                             MPI_File *fp, MPI_Request *request,
                                             int svi,
                                             int lvi,
                                             int layout_start, int layout_end,
                                             int agg_io_level, int file_zero, int mode);

static PIDX_return_code PIDX_global_aggregate(PIDX_io file, PIDX_agg_id** agg_id,
                                              Agg_buffer** agg_buffer,
                                              PIDX_block_layout* block_layout_by_level,
                                              int svi,
                                              int evi,
                                              int lvi,
                                              int layout_start,
                                              int agg_io_level, int file_status, int agg_mode, int mode);

static PIDX_return_code PIDX_shared_block_aggregate(PIDX_io file, PIDX_shared_block_agg_id* agg_id, Agg_buffer** agg_buffer, PIDX_block_layout* block_layout_by_level, int svi, int mode);

PIDX_return_code data_io(PIDX_io file, int gi, int local_var_index, int start_index, int end_index, int mode)
{
  int ret;
  PIDX_variable_group var_grp = file->idx->variable_grp[gi];
  idx_dataset_derived_metadata idx = file->idx_d;

  ret = PIDX_global_async_io(file, file->f0_io_id,
                             idx->f0_agg_buffer,
                             var_grp->f0_block_layout_by_level,
                             idx->fp_file_zero, idx->request_file_zero,
                             start_index,
                             local_var_index,
                             var_grp->f0_start_layout_index, var_grp->f0_end_layout_index,
                             var_grp->agg_l_f0, 0, mode);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }
#if 1
  ret = PIDX_global_async_io(file, file->shared_io_id,
                             idx->shared_agg_buffer,
                             var_grp->shared_block_layout_by_level,
                             idx->fp_shared,
                             idx->request_shared,
                             start_index,
                             local_var_index,
                             var_grp->shared_start_layout_index, var_grp->shared_end_layout_index,
                             var_grp->agg_l_shared, 1, mode);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }
#endif
  ret = PIDX_global_async_io(file, file->nshared_io_id,
                             idx->nshared_agg_buffer,
                             var_grp->nshared_block_layout_by_level,
                             idx->fp_non_shared,
                             idx->request_non_shared,
                             start_index,
                             local_var_index,
                             var_grp->nshared_start_layout_index, var_grp->nshared_end_layout_index,
                             var_grp->agg_l_nshared, 2, mode);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

  return PIDX_success;
}


static PIDX_return_code PIDX_global_async_io(PIDX_io file, PIDX_file_io_id **io_id, Agg_buffer **agg_buffer, PIDX_block_layout* block_layout_by_level,  MPI_File *fp, MPI_Request *request, int svi, int lvi, int layout_start, int layout_end, int agg_io_level, int file_zero, int mode)
{
  int j;
  int ret = 0;
  int j_1 = 0;
  svi = svi - lvi;
  //printf("svi = %d\n", svi);

  for(j = layout_start ; j < agg_io_level; j++)
  {
    j_1 = j - layout_start;
    Agg_buffer temp_agg = agg_buffer[svi][j_1];
    PIDX_block_layout temp_layout = block_layout_by_level[j_1];

    io_id[svi][j_1] = PIDX_file_io_init(file->idx, file->idx_d, file->idx_c, svi + lvi, svi + lvi);

    if (file->idx_dbg->debug_do_io == 1)
    {
      if (file_zero == 0)
      {
        ret = PIDX_async_aggregated_io(io_id[svi][j_1], temp_agg, temp_layout, &(request[j_1]), &(fp[j_1]), file->idx->filename_template_file_zero, mode);
      }
      else if (file_zero == 1)
      {
        ret = PIDX_async_aggregated_io(io_id[svi][j_1], temp_agg, temp_layout, &(request[j_1]), &(fp[j_1]), file->idx->filename_template_partition, mode);
      }
      else if (file_zero == 2)
      {
        if (file->idx_d->io_mode == 1)
          ret = PIDX_async_aggregated_io(io_id[svi][j_1], temp_agg, temp_layout, &(request[j_1]), &(fp[j_1]), file->idx->filename_template_partition, mode);

        else
          ret = PIDX_async_aggregated_io(io_id[svi][j_1], temp_agg, temp_layout, &(request[j_1]), &(fp[j_1]), file->idx->filename_template, mode);
      }
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_io;
      }
    }
  }

  return PIDX_success;
}


PIDX_return_code data_aggregate(PIDX_io file, int gi, int local_var_index, int start_index, int end_index, int agg_mode, int mode )
{
  int ret = 0;
  lgi = gi;

  PIDX_variable_group var_grp = file->idx->variable_grp[gi];
  idx_dataset_derived_metadata idx = file->idx_d;

  //fprintf(stderr, "SHARED %d %d\n", var_grp->shared_start_layout_index, var_grp->agg_l_shared);
  //fprintf(stderr, "NON - SHARED %d %d\n", var_grp->nshared_start_layout_index, var_grp->agg_l_nshared);

  ret = PIDX_global_aggregate(file, file->f0_agg_id,
                              idx->f0_agg_buffer,
                              var_grp->f0_block_layout_by_level,
                              start_index,
                              end_index,
                              local_var_index,
                              var_grp->f0_start_layout_index,
                              var_grp->agg_l_f0, 2, agg_mode, mode);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

#if 1
  ret = PIDX_global_aggregate(file, file->shared_agg_id,
                              idx->shared_agg_buffer,
                              var_grp->shared_block_layout_by_level,
                              start_index,
                              end_index,
                              local_var_index,
                              var_grp->shared_start_layout_index,
                              var_grp->agg_l_shared, 0, agg_mode, mode);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }
#endif
#if 1
  ret = PIDX_global_aggregate(file, file->nshared_agg_id,
                              idx->nshared_agg_buffer,
                              var_grp->nshared_block_layout_by_level,
                              start_index,
                              end_index,
                              local_var_index,
                              var_grp->nshared_start_layout_index,
                              var_grp->agg_l_nshared, 1, agg_mode, mode);
  if (ret != PIDX_success)
  {
    fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
    return PIDX_err_file;
  }

#if 0
  if (agg_mode == AGG_PERFORM)
  {
    ret = PIDX_shared_block_aggregate(file,
                                file->shared_block_agg_id,
                                idx->shared_agg_buffer,
                                var_grp->shared_block_layout_by_level,
                                start_index, mode);
    if (ret != PIDX_success)
    {
      fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
      return PIDX_err_file;
    }
  }
#endif

#endif

  return PIDX_success;
}

static PIDX_return_code PIDX_shared_block_aggregate(PIDX_io file, PIDX_shared_block_agg_id* agg_id, Agg_buffer** agg_buffer, PIDX_block_layout* block_layout_by_level, int svi, int mode)
{
  PIDX_time time = file->idx_d->time;

  agg_id[svi] = PIDX_shared_block_agg_init(file->idx, file->idx_d, file->idx_c, svi, svi);
  PIDX_shared_block_agg_buf_create(agg_id[svi], agg_buffer[svi][0]);
  PIDX_shared_block_agg_global_and_local(agg_id[svi], agg_buffer[svi][0], block_layout_by_level[0],  mode);
  PIDX_shared_block_agg_buf_destroy(agg_id[svi], agg_buffer[svi][0]);
  PIDX_shared_block_agg_finalize(agg_id[svi]);

  return PIDX_success;
}


static PIDX_return_code PIDX_global_aggregate(PIDX_io file, PIDX_agg_id** agg_id,
                                              Agg_buffer** agg_buffer,
                                              PIDX_block_layout* block_layout_by_level,
                                              int svi,
                                              int evi,
                                              int lvi,
                                              int layout_start,
                                              int agg_io_level, int file_status, int agg_mode, int mode)
{
  int j;
  int ret = 0;
  PIDX_time time = file->idx_d->time;

  int j_1 = 0;
  svi = svi - lvi;
  evi = evi - lvi;

  for (j = layout_start; j < agg_io_level; j++)
  {
    j_1 = j - layout_start;
    if (agg_mode == AGG_SETUP_AND_PERFORM || agg_mode == AGG_SETUP)
    {
      time->agg_init_start[lgi][svi][j] = PIDX_get_time();
      //printf("svi %d\n", svi);
      //agg_id[svi][j_1] = PIDX_agg_init(file->idx, file->idx_d, file->idx_c, svi + lvi, svi + lvi, lvi);
      agg_id[svi][j_1] = PIDX_agg_init(file->idx, file->idx_d, file->idx_c, svi + lvi, evi + lvi, lvi);
      agg_buffer[svi][j_1] = malloc(sizeof(*(agg_buffer[svi][j_1])));
      memset(agg_buffer[svi][j_1], 0, sizeof(*(agg_buffer[svi][j_1])));

      agg_buffer[svi][j_1]->file_number = -1;
      agg_buffer[svi][j_1]->var_number = -1;
      agg_buffer[svi][j_1]->sample_number = -1;

      agg_buffer[svi][j_1]->no_of_aggregators = 0;
      agg_buffer[svi][j_1]->aggregator_interval = 0;
      agg_buffer[svi][j_1]->agg_f = 1;
      time->agg_init_end[lgi][svi][j] = PIDX_get_time();

      time->agg_meta_start[lgi][svi][j] = PIDX_get_time();
      ret = PIDX_agg_meta_data_create(agg_id[svi][j_1], agg_buffer[svi][j_1], block_layout_by_level[j_1]);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_rst;
      }
      time->agg_meta_end[lgi][svi][j] = PIDX_get_time();

      time->agg_buf_start[lgi][svi][j] = PIDX_get_time();

      ret = PIDX_agg_random_buf_create_multiple_level(agg_id[svi][j_1], agg_buffer[svi][j_1], block_layout_by_level[j_1], j, svi, file_status);

      //ret = PIDX_agg_buf_create_localized_aggregation(agg_id[svi][j_1], agg_buffer[svi][j_1], block_layout_by_level[j_1], j, svi, file_status);

      //ret = PIDX_agg_buf_create_local_uniform_dist(agg_id[svi][j_1], agg_buffer[svi][j_1], block_layout_by_level[j_1], j, svi, file_status);

      // working
      //ret = PIDX_agg_buf_create_global_uniform_dist(agg_id[svi][j_1], agg_buffer[svi][j_1], block_layout_by_level[j_1], j, svi, file_status);
      //ret = PIDX_agg_buf_create_multiple_level(agg_id[svi][j_1], agg_buffer[svi][j_1], block_layout_by_level[j_1], j, svi, file_status);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_rst;
      }
      time->agg_buf_end[lgi][svi][j] = PIDX_get_time();
    }

    if (agg_mode == AGG_SETUP_AND_PERFORM || agg_mode == AGG_PERFORM)
    {
      if (file->idx_dbg->debug_do_agg == 1)
      {
        time->agg_start[lgi][svi][j] = PIDX_get_time();
        ret = PIDX_agg_global_and_local(agg_id[svi][j_1], agg_buffer[svi][j_1], j, block_layout_by_level[j_1], mode);
        if (ret != PIDX_success)
        {
          fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
          return PIDX_err_rst;
        }
        time->agg_end[lgi][svi][j] = PIDX_get_time();

        time->agg_compress_start[lgi][svi][j] = PIDX_get_time();
        ret = PIDX_agg_buffer_compress(agg_id[svi][j_1], agg_buffer[svi][j_1], j, block_layout_by_level[j_1], mode);
        if (ret != PIDX_success)
        {
          fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
          return PIDX_err_rst;
        }
        time->agg_compress_end[lgi][svi][j] = PIDX_get_time();

      }


      time->agg_meta_cleanup_start[lgi][svi][j] = PIDX_get_time();
      ret = PIDX_agg_meta_data_destroy(agg_id[svi][j_1], block_layout_by_level[j_1]);
      if (ret != PIDX_success)
      {
        fprintf(stderr,"File %s Line %d\n", __FILE__, __LINE__);
        return PIDX_err_rst;
      }
      time->agg_meta_cleanup_end[lgi][svi][j] = PIDX_get_time();
    }
  }

  return PIDX_success;
}

