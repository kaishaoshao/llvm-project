/// 零寄存器（硬连线为0）
/// - 无法被写入操作修改
/// - 调试符号编号：DWARF 0
def ZERO : Register<"zero">, DwarfRegNum<[0]> {
    let Namespace = "RISCVCPU";
}

def GPR : RegisterClass<"RISCVCPU", [i32], 32, (add ZERO)> {
    // 命名空间：RISCVCPU
    // 支持32位整数操作
    // 包含ZERO寄存器[2,7](@ref)
}