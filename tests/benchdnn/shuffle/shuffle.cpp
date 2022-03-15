/*******************************************************************************
* Copyright 2018-2022 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "oneapi/dnnl/dnnl.h"

#include "utils/parallel.hpp"

#include "dnnl_common.hpp"
#include "dnnl_memory.hpp"

#include "shuffle/shuffle.hpp"

namespace shuffle {

int fill_src(const prb_t *prb, dnn_mem_t &mem_dt, dnn_mem_t &mem_fp) {
    auto get_range = [](const dnnl_data_type_t dt) {
        if (dt == dnnl_s8 || dt == dnnl_u8)
            return 256;
        else if (dt == dnnl_bf16 || dt == dnnl_f16)
            return 128;
        return 1024;
    };

    const auto nelems = mem_fp.nelems();
    const int range = get_range(prb->dt);
    const int f_min = prb->dt == dnnl_u8 ? 0 : -range / 2;

    benchdnn_parallel_nd(nelems, [&](int64_t i) {
        const float gen = ((97 * i) + 101) % range;
        const float value = (prb->dt == dnnl_bf16 || prb->dt == dnnl_f16)
                ? (f_min + gen) / range
                : (f_min + gen) * (1.0f + 4.0f / range);
        mem_fp.set_elem(i, round_to_nearest_representable(prb->dt, value));
    });

    SAFE(mem_dt.reorder(mem_fp), WARN);

    return OK;
}

static int init_pd(dnnl_engine_t engine, const prb_t *prb,
        dnnl_primitive_desc_t &spd, res_t *res, dir_t dir,
        const_dnnl_primitive_desc_t hint) {
    dnnl_shuffle_desc_t sd;

    auto data_d = dnn_mem_t::init_md(
            prb->ndims, prb->dims.data(), prb->dt, prb->tag);

    if (prb->dir & FLAG_FWD) {
        auto prop_kind = prb->dir & FLAG_INF ? dnnl_forward_inference
                                             : dnnl_forward_training;

        DNN_SAFE(dnnl_shuffle_forward_desc_init(
                         &sd, prop_kind, &data_d, prb->axis, prb->group),
                WARN);
    } else {
        dnnl_memory_desc_t diff_data_d;
        DNN_SAFE(dnnl_memory_desc_init_by_tag(&diff_data_d, prb->ndims,
                         prb->dims.data(), prb->dt, dnnl_format_tag_any),
                WARN);

        DNN_SAFE(dnnl_shuffle_backward_desc_init(
                         &sd, &diff_data_d, prb->axis, prb->group),
                WARN);
    }

    dnnl_primitive_desc_t hint_fwd_pd_ {};
    dnnl_status_t status = dnnl_success;
    if (prb->dir & FLAG_BWD) {
        dnnl_shuffle_desc_t sd_fwd;
        DNN_SAFE(dnnl_shuffle_forward_desc_init(&sd_fwd, dnnl_forward_training,
                         &data_d, prb->axis, prb->group),
                WARN);

        status = dnnl_primitive_desc_create(
                &hint_fwd_pd_, &sd_fwd, nullptr, engine, nullptr);
        if (status == dnnl_unimplemented) return res->state = UNIMPLEMENTED, OK;
    }
    auto hint_fwd_pd = make_benchdnn_dnnl_wrapper(hint_fwd_pd_);
    SAFE(status, WARN);

    auto dnnl_attr = make_benchdnn_dnnl_wrapper(
            create_dnnl_attr(prb->attr, attr_args_t()));

    status = dnnl_primitive_desc_create(
            &spd, &sd, dnnl_attr, engine, hint_fwd_pd);

    if (status == dnnl_unimplemented) return res->state = UNIMPLEMENTED, OK;
    SAFE(status, WARN);

    res->impl_name = query_impl_info(spd);
    BENCHDNN_PRINT(5, "oneDNN implementation: %s\n", res->impl_name.c_str());

    SAFE(check_pd_w_and_wo_attr(res, prb->attr, sd), WARN);

    return OK;
}

void check_known_skipped_case(const prb_t *prb, res_t *res) {
    check_known_skipped_case_common({prb->dt}, prb->dir, res);
    if (res->state == SKIPPED) return;

    if (is_nvidia_gpu()) {
        res->state = SKIPPED, res->reason = CASE_NOT_SUPPORTED;
        return;
    }
}

void setup_cmp(compare::compare_t &cmp, const prb_t *prb, data_kind_t kind,
        const args_t &ref_args) {}

int doit(const prb_t *prb, res_t *res) {
    if (bench_mode == LIST) return res->state = LISTED, OK;

    check_known_skipped_case(prb, res);
    check_sum_post_ops(prb->attr, res);
    if (res->state == SKIPPED) return OK;

    benchdnn_dnnl_wrapper_t<dnnl_primitive_t> prim;
    SAFE(init_prim(prim, init_pd, prb, res), WARN);
    if (res->state == SKIPPED || res->state == UNIMPLEMENTED) return OK;

    const_dnnl_primitive_desc_t const_pd;
    DNN_SAFE(dnnl_primitive_get_primitive_desc(prim, &const_pd), CRIT);

    if (check_mem_size(const_pd) != OK) {
        return res->state = SKIPPED, res->reason = NOT_ENOUGH_RAM, OK;
    }

    const auto q = [&](int index = 0) -> const dnnl_memory_desc_t & {
        return *dnnl_primitive_desc_query_md(
                const_pd, dnnl_query_exec_arg_md, index);
    };

    const auto &data_md
            = prb->dir & FLAG_FWD ? q(DNNL_ARG_SRC) : q(DNNL_ARG_DIFF_SRC);
    const auto &scratchpad_md = q(DNNL_ARG_SCRATCHPAD);
    const auto &test_engine = get_test_engine();
    const auto &ref_engine = get_cpu_engine();

    dnn_mem_t src_fp(data_md, dnnl_f32, tag::abx, ref_engine);
    dnn_mem_t src_dt(data_md, test_engine);

    dnn_mem_t dst_fp(data_md, dnnl_f32, tag::abx, ref_engine);
    dnn_mem_t dst_dt(data_md, test_engine);

    dnn_mem_t scratchpad_dt(scratchpad_md, test_engine);

    args_t args, ref_args;

    if (prb->dir & FLAG_FWD) {
        SAFE(fill_src(prb, src_dt, src_fp), WARN);

        args.set(DNNL_ARG_SRC, src_dt);
        args.set(DNNL_ARG_DST, dst_dt);
        args.set(DNNL_ARG_SCRATCHPAD, scratchpad_dt);

        SAFE(execute_and_wait(prim, args, res), WARN);

        if (is_bench_mode(CORR)) {
            ref_args.set(DNNL_ARG_SRC, src_fp);
            ref_args.set(DNNL_ARG_DST, dst_fp);

            check_correctness(prb, {DST}, args, ref_args, setup_cmp, res);
        }
    } else {
        SAFE(fill_src(prb, dst_dt, dst_fp), WARN);

        args.set(DNNL_ARG_DIFF_DST, dst_dt);
        args.set(DNNL_ARG_DIFF_SRC, src_dt);
        args.set(DNNL_ARG_SCRATCHPAD, scratchpad_dt);

        SAFE(execute_and_wait(prim, args, res), WARN);

        if (is_bench_mode(CORR)) {
            ref_args.set(DNNL_ARG_DIFF_DST, dst_fp);
            ref_args.set(DNNL_ARG_DIFF_SRC, src_fp);

            check_correctness(prb, {SRC}, args, ref_args, setup_cmp, res);
        }
    }

    return measure_perf(res, prim, args);
}

} // namespace shuffle
