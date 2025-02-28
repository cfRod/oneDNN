ReduceSum{#dev_guide_op_reducesum}
==================================

## General

ReduceSum operation performs the reduction with addition on a given src data
along dimensions specified by axes.

Take channel axis = 0 and keep_dims = True as an example:
  \f[ {dst}_{0,\cdots,\cdots} =
  \sum\limits_{i}src_{i,\cdots,\cdots} ,i \in [0,channelNum-1] \f]

## Operation attributes

| Attribute Name                                    | Description                                                                                                                                                                                                                                                                                                                                                                          | Value Type | Supported Values                                                                               | Required or Optional |
|:--------------------------------------------------|:-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|:-----------|:-----------------------------------------------------------------------------------------------|:---------------------|
| [axes](@ref dnnl::graph::op::attr::axes)          | Specify indices of src tensor, along which the reduction is performed. If axes is a list, reduce over all of them. If axes is empty, corresponds to the identity operation. If axes contains all dimensions of src tensor, a single reduction value is calculated for the entire src tensor. Exactly one of attribute `axes` and the second input tensor `axes` should be available. | s64        | A s64 list values which is in the range of [-r, r-1] where r = rank(src). Empty list(default)  | Optional             |
|[keep_dims](@ref dnnl::graph::op::attr::keep_dims) | If set to `true` it holds axes that are used for reduction. For each such axes, dst dimension is equal to 1.                                                                                                                                                                                                                                                                         | bool       | `true`,`false`(default)                                                                        | Optional             |

## Execution arguments

The inputs and outputs must be provided according to below index order when
constructing an operation.

### Inputs

| Index | Argument Name | Required or Optional |
|:------|:--------------|:---------------------|
| 0     | `src`         | Required             |
| 1     | `axes`        | Optional             |

@note `axes` is an 1-D tensor specifying the axis along which the reduction is
performed. 1D tensor of unique elements. The range of elements is [-r, r-1],
where r is the rank of src tensor. Exactly one of attribute axes and the second
input tensor axes should be available.

### Outputs

| Index | Argument Name | Required or Optional |
|:----- |:--------------|:---------------------|
| 0     | `dst`         | Required             |

@note The result of ReduceSum function applied to src tensor. shape[i] =
shapeOf[data](i) for all i that is not in the list of axes from the second
input. For dimensions from axes, shape[i] == 1 if keep_dims == True, or i-th
dimension is removed from the dst otherwise.

## Supported data types

ReduceSum operation supports the following data type combinations.

| Source/Destination | Axes |
|:-------------------|:-----|
| f32                | s32  |
| bf16               | s32  |
| f16                | s32  |
