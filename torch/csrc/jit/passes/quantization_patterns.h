#pragma once

#include <string>
#include <unordered_map>
#include <torch/csrc/jit/ir/ir.h>
#include <torch/csrc/jit/ir/subgraph_matcher.h>
#include <torch/csrc/jit/passes/subgraph_rewrite.h>

namespace torch {
namespace jit {

struct QuantFusionInfo {
  std::string quantized_op_name;
  std::string pattern;
  std::string replacement;
  std::function<bool(const Match&, const std::unordered_map<std::string, Value*>&)> filter = [](const Match&, const std::unordered_map<std::string, Value*>&) {
     return true;
  };
};

std::vector<QuantFusionInfo> quant_fusion_pattern_and_replacements() {
  std::string conv2d = R"(
graph(%a_quant, %packed_params, %r_scale, %r_zero_point, %r_dtype, %stride, %padding, %dilation, %groups):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::conv2d_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %r = aten::conv2d(%a_dequant, %w_dequant, %b, %stride, %padding, %dilation, %groups)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string quantized_conv2d = R"(
graph(%a_quant, %packed_params, %r_scale, %r_zero_point, %r_dtype, %stride, %padding, %dilation, %groups):
        %r_quant = quantized::conv2d(%a_quant, %packed_params, %stride, %padding, %dilation, %groups, %r_scale, %r_zero_point)
        return (%r_quant) )";

  std::string addmm = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype, %4):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::linear_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %w_dequant_t = aten::t(%w_dequant)
        %r = aten::addmm(%b, %a_dequant, %w_dequant_t, %4, %4)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string matmul_with_bias = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype, %4):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::linear_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %w_dequant_t = aten::t(%w_dequant)
        %output = aten::matmul(%a_dequant, %w_dequant_t)
        %r = aten::add_(%output, %b, %4)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string quantized_linear = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype, %4):
        %r = quantized::linear(%a_quant, %packed_params, %r_scale, %r_zero_point)
        return (%r) )";

  std::string matmul_no_bias = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::linear_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %w_dequant_t = aten::t(%w_dequant)
        %r = aten::matmul(%a_dequant, %w_dequant_t)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string quantized_linear_no_bias = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype):
        %r = quantized::linear(%a_quant, %packed_params, %r_scale, %r_zero_point)
        return (%r) )";

  std::string conv2d_relu = R"(
graph(%a_quant, %packed_params, %r_scale, %r_zero_point, %r_dtype, %stride, %padding, %dilation, %groups):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::conv2d_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %conv_out = aten::conv2d(%a_dequant, %w_dequant, %b, %stride, %padding, %dilation, %groups)
        %r = aten::relu(%conv_out)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string conv2d_inplace_relu = R"(
graph(%a_quant, %packed_params, %r_scale, %r_zero_point, %r_dtype, %stride, %padding, %dilation, %groups):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::conv2d_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %conv_out = aten::conv2d(%a_dequant, %w_dequant, %b, %stride, %padding, %dilation, %groups)
        %r = aten::relu_(%conv_out)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string quantized_conv2d_relu = R"(
graph(%a_quant, %packed_params, %r_scale, %r_zero_point, %r_dtype, %stride, %padding, %dilation, %groups):
        %r_quant = quantized::conv2d_relu(%a_quant, %packed_params, %stride, %padding, %dilation, %groups, %r_scale, %r_zero_point)
        return (%r_quant) )";

  std::string add_relu = R"(
graph(%a_quant, %b_quant, %scale, %zero_point, %dtype):
         %alpha = prim::Constant[value=1]()
         %a_dequant = aten::dequantize(%a_quant)
         %b_dequant = aten::dequantize(%b_quant)
         %r_add = aten::add_(%a_dequant, %b_dequant, %alpha)
         %r_relu = aten::relu(%r_add)
         %r = aten::quantize_per_tensor(%r_relu, %scale, %zero_point, %dtype)
         return (%r) )";

  std::string add_inplace_relu = R"(
graph(%a_quant, %b_quant, %scale, %zero_point, %dtype):
         %alpha = prim::Constant[value=1]()
         %a_dequant = aten::dequantize(%a_quant)
         %b_dequant = aten::dequantize(%b_quant)
         %r_add = aten::add_(%a_dequant, %b_dequant, %alpha)
         %r_relu = aten::relu_(%r_add)
         %r = aten::quantize_per_tensor(%r_relu, %scale, %zero_point, %dtype)
         return (%r) )";

  std::string quantized_add_relu = R"(
graph(%a_quant, %b_quant, %scale, %zero_point, %dtype):
         %r = quantized::add_relu(%a_quant, %b_quant, %scale, %zero_point)
         return (%r) )";

  std::string aten_linear = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype):
        %a_dequant = aten::dequantize(%a_quant)
        %w_quant : Tensor, %b : Tensor? = quantized::linear_unpack(%packed_params)
        %w_dequant = aten::dequantize(%w_quant)
        %r = aten::linear(%a_dequant, %w_dequant, %b)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string quantized_aten_linear = R"(
graph(%packed_params, %a_quant, %r_scale, %r_zero_point, %r_dtype):
        %r = quantized::linear(%a_quant, %packed_params, %r_scale, %r_zero_point)
        return (%r) )";

  std::string cat = R"(
graph(%input_quant, %dim, %r_scale, %r_zero_point, %r_dtype):
        %input_dequant = aten::dequantize(%input_quant)
        %r = aten::cat(%input_dequant, %dim)
        %r_quant = aten::quantize_per_tensor(%r, %r_scale, %r_zero_point, %r_dtype)
        return (%r_quant) )";

  std::string quantized_cat = R"(
graph(%input_quant, %dim, %r_scale, %r_zero_point, %r_dtype):
         %r_quant = quantized::cat(%input_quant, %dim, %r_scale, %r_zero_point)
         return (%r_quant) )";

  std::string add = R"(
graph(%a_quant, %b_quant, %alpha, %scale, %zero_point, %dtype):
         %a_dequant = aten::dequantize(%a_quant)
         %b_dequant = aten::dequantize(%b_quant)
         %r_add = aten::add(%a_dequant, %b_dequant, %alpha)
         %r = aten::quantize_per_tensor(%r_add, %scale, %zero_point, %dtype)
         return (%r) )";

  // TODO: add %dtype after when https://github.com/pytorch/pytorch/issues/34351
  // is fixed
  // TODO: add filter for %alpha
  std::string quantized_add = R"(
graph(%a_quant, %b_quant, %alpha, %scale, %zero_point, %dtype):
         %r_add = quantized::add(%a_quant, %b_quant, %scale, %zero_point)
         return (%r_add) )";

  std::string inplace_add = R"(
graph(%a_quant, %b_quant, %alpha, %scale, %zero_point, %dtype):
         %a_dequant = aten::dequantize(%a_quant)
         %b_dequant = aten::dequantize(%b_quant)
         %r_add = aten::add_(%a_dequant, %b_dequant, %alpha)
         %r = aten::quantize_per_tensor(%r_add, %scale, %zero_point, %dtype)
         return (%r) )";
  // We don't have quantized inplace add right now

  // quantized::add_scalar
  std::string add_scalar = R"(
graph(%a_quant, %b_scalar, %alpha):
         %r = aten::add(%a_quant, %b_scalar, %alpha)
         return (%r) )";

  std::string quantized_add_scalar = R"(
graph(%a_quant, %b_scalar, %alpha):
         %r = quantized::add_scalar(%a_quant, %b_scalar)
         return (%r) )";

  // filter that checks %alpha is constant 1 and %b_scalar is a scalar
  auto add_scalar_filter = [](const Match& match,
                   const std::unordered_map<std::string, Value*>& vmap) {
    const auto& match_vmap = match.values_map;
    auto alpha = toIValue(match_vmap.at(vmap.at("alpha")));
    auto b_scalar = match_vmap.at(vmap.at("b_scalar"));
    return alpha && alpha->isInt() && alpha->toInt() == 1 &&
      b_scalar->type()->isSubtypeOf(NumberType::get());
  };

  // quantized::add_scalar_out
  std::string add_scalar_out = R"(
graph(%a_quant, %b_scalar, %alpha):
         %r = aten::add_(%a_quant, %b_scalar, %alpha)
         return (%r) )";

  std::string quantized_add_scalar_out = R"(
graph(%a_quant, %b_scalar, %alpha):
         %r = quantized::add_scalar_out(%a_quant, %b_scalar, %a_quant)
         return (%r) )";

  return {
      {"quantized::conv2d", conv2d, quantized_conv2d},
      {"quantized::conv2d_relu", conv2d_relu, quantized_conv2d_relu},
      {"quantized::conv2d_relu", conv2d_inplace_relu, quantized_conv2d_relu},
      {"quantized::linear", addmm, quantized_linear},
      {"quantized::linear", matmul_with_bias, quantized_linear},
      {"quantized::linear", matmul_no_bias, quantized_linear_no_bias},
      {"quantized::linear", aten_linear, quantized_aten_linear},
      {"quantized::add_relu", add_relu, quantized_add_relu},
      {"quantized::add_relu", add_inplace_relu, quantized_add_relu},
      {"quantized::add", add, quantized_add},
      {"quantized::add", inplace_add, quantized_add},
      {"quantized::cat", cat, quantized_cat},
      {"quantized::add_scalar", add_scalar,
       quantized_add_scalar, add_scalar_filter},
      {"quantized::add_scalar_out", add_scalar_out,
       quantized_add_scalar_out, add_scalar_filter},
  };
}

} // namespace jit
} // namespace torch
