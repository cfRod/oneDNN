TanhBackward {#dev_guide_op_tanhbackward}
=========================================

## General

TanhBackward operation computes gradient for Tanh.

## Operation attributes

| Attribute Name                                 | Description                                                                           | Value Type | Supported Values          | Required or Optional |
|:-----------------------------------------------|:--------------------------------------------------------------------------------------|:-----------|:--------------------------| :--------------------|
| [use_dst](@ref dnnl::graph::op::attr::use_dst) | If true, use `dst` of Tanh operation to calculate the gradient. Otherwise, use `src`. | bool       | `true` (default), `false` | Optional             |

## Execution arguments

The inputs and outputs must be provided according to below index order when
constructing an operation.

### Inputs

| Index | Argument Name | Required or Optional |
|:------|:--------------|:---------------------|
| 0     | `src` / `dst` | Required             |
| 1     | `diff_dst`    | Required             |

### Outputs

| Index | Argument Name | Required or Optional |
|:------|:--------------|:---------------------|
| 0     | `diff_src`    | Required             |

## Supported data types

TanhBackward operation supports the following data type combinations.

| Src  | Diff_dst | Diff_src |
|:-----|:---------|:---------|
| f32  | f32      | f32      |
| f16  | f16      | f16      |
| bf16 | bf16     | bf16     |
