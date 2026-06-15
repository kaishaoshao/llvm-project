let Namespace = "CoralNPU" in {
  class CoralNPUReg<bits<5> Enc, string n, list<string> alt = []>
    : Register<n> {
      let HWEncoding{4-0} = Enc;
      let AltNames = alt;
    }

  class CoralNPUGPRReg<bits<5> Enc, string n, list<string> alt = []>
    : CoralNPUReg<Enc, n, alt>;

  let Namespace = "CoralNPU" in {
    def ZERO : CoralNPUGPRReg<0, "x0",  ["zero"]>;
    def RA   : CoralNPUGPRReg<0, "x1",  ["ra"]>;
    def SP   : CoralNPUGPRReg<0, "x2",  ["sp"]>;
    def A0   : CoralNPUGPRReg<0, "x10", ["a0"]>;
    def A1   : CoralNPUGPRReg<0, "x11", ["a1"]>;
  }

  def XLenVT : ValueTypeByHwMode<[DefaultMode, Coral64Bit], [i32, i64]>;

  def GPR : RegisterClass<"CoralNPU", [XLenVT], 32, (add ZERO, RA, SP, A0, A1)> {
    let RegInfos = RegInfoByHwMode<
      [DefaultMode, Coral64Bit],
      [RegInfo<32,32,32>, RegInfo<64,64,64>]
    >;
  }
}
