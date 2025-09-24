include "llvm/Target/Target.td"

include "NanoRegisterInfo.td"

// 定义名为 “generic” 的通用处理器模型
// NoSchedModel 表示没有特定的调度模型
// []：空列表表示无额外处理器特性。通常用于基础的配置或默认的模型
def : ProcessorModel<"generic", NoSchedModel, []>;

def Nano : Target {

}
