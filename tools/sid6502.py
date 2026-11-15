"""A small 6502, enough to run a SID tune's init and play and watch it.

tools/mksid.py runs every tune the game will load through this before it
goes on the disk: what it writes, what it reads, where it jumps, how long a
call takes. Official NMOS opcodes only; anything else stops the run, since a
tune that needs one cannot be checked (and the check is the point). Cycle
counts are the base ones, without the extra cycle for crossing a page, so
they are a slight underestimate.

    cpu = CPU(mem, on_read, on_write, on_fetch)
    cycles = cpu.call(addr, a=song)   # runs until the matching RTS

on_read(addr) may return a value to stand in for memory (I/O) or None;
on_write(addr, value) and on_fetch(pc) raise Stop to refuse.
"""


class Stop(Exception):
    pass


# (mnemonic, mode, cycles) by opcode.
MODES = {}
_TABLE = """
00 BRK imp 7  01 ORA izx 6  05 ORA zp 3   06 ASL zp 5   08 PHP imp 3  09 ORA imm 2
0A ASL acc 2  0D ORA abs 4  0E ASL abs 6  10 BPL rel 2  11 ORA izy 5  15 ORA zpx 4
16 ASL zpx 6  18 CLC imp 2  19 ORA aby 4  1D ORA abx 4  1E ASL abx 7  20 JSR abs 6
21 AND izx 6  24 BIT zp 3   25 AND zp 3   26 ROL zp 5   28 PLP imp 4  29 AND imm 2
2A ROL acc 2  2C BIT abs 4  2D AND abs 4  2E ROL abs 6  30 BMI rel 2  31 AND izy 5
35 AND zpx 4  36 ROL zpx 6  38 SEC imp 2  39 AND aby 4  3D AND abx 4  3E ROL abx 7
40 RTI imp 6  41 EOR izx 6  45 EOR zp 3   46 LSR zp 5   48 PHA imp 3  49 EOR imm 2
4A LSR acc 2  4C JMP abs 3  4D EOR abs 4  4E LSR abs 6  50 BVC rel 2  51 EOR izy 5
55 EOR zpx 4  56 LSR zpx 6  58 CLI imp 2  59 EOR aby 4  5D EOR abx 4  5E LSR abx 7
60 RTS imp 6  61 ADC izx 6  65 ADC zp 3   66 ROR zp 5   68 PLA imp 4  69 ADC imm 2
6A ROR acc 2  6C JMP ind 5  6D ADC abs 4  6E ROR abs 6  70 BVS rel 2  71 ADC izy 5
75 ADC zpx 4  76 ROR zpx 6  78 SEI imp 2  79 ADC aby 4  7D ADC abx 4  7E ROR abx 7
81 STA izx 6  84 STY zp 3   85 STA zp 3   86 STX zp 3   88 DEY imp 2  8A TXA imp 2
8C STY abs 4  8D STA abs 4  8E STX abs 4  90 BCC rel 2  91 STA izy 6  94 STY zpx 4
95 STA zpx 4  96 STX zpy 4  98 TYA imp 2  99 STA aby 5  9A TXS imp 2  9D STA abx 5
A0 LDY imm 2  A1 LDA izx 6  A2 LDX imm 2  A4 LDY zp 3   A5 LDA zp 3   A6 LDX zp 3
A8 TAY imp 2  A9 LDA imm 2  AA TAX imp 2  AC LDY abs 4  AD LDA abs 4  AE LDX abs 4
B0 BCS rel 2  B1 LDA izy 5  B4 LDY zpx 4  B5 LDA zpx 4  B6 LDX zpy 4  B8 CLV imp 2
B9 LDA aby 4  BA TSX imp 2  BC LDY abx 4  BD LDA abx 4  BE LDX aby 4  C0 CPY imm 2
C1 CMP izx 6  C4 CPY zp 3   C5 CMP zp 3   C6 DEC zp 5   C8 INY imp 2  C9 CMP imm 2
CA DEX imp 2  CC CPY abs 4  CD CMP abs 4  CE DEC abs 6  D0 BNE rel 2  D1 CMP izy 5
D5 CMP zpx 4  D6 DEC zpx 6  D8 CLD imp 2  D9 CMP aby 4  DD CMP abx 4  DE DEC abx 7
E0 CPX imm 2  E1 SBC izx 6  E4 CPX zp 3   E5 SBC zp 3   E6 INC zp 5   E8 INX imp 2
E9 SBC imm 2  EA NOP imp 2  EC CPX abs 4  ED SBC abs 4  EE INC abs 6  F0 BEQ rel 2
F1 SBC izy 5  F5 SBC zpx 4  F6 INC zpx 6  F8 SED imp 2  F9 SBC aby 4  FD SBC abx 4
FE INC abx 7
"""
_t = _TABLE.split()
for _i in range(0, len(_t), 4):
    MODES[int(_t[_i], 16)] = (_t[_i + 1], _t[_i + 2], int(_t[_i + 3]))

C, Z, I, D, B, U, V, N = 1, 2, 4, 8, 16, 32, 64, 128
SENTINEL = 0xFFFF       # where call()'s outermost RTS lands


class CPU:
    def __init__(self, mem, on_read=None, on_write=None, on_fetch=None):
        self.mem = mem
        self.on_read = on_read or (lambda a: None)
        self.on_write = on_write or (lambda a, v: None)
        self.on_fetch = on_fetch or (lambda pc: None)
        self.a = self.x = self.y = 0
        self.sp = 0xFF
        self.p = U | I
        self.low_sp = 0xFF      # the deepest the stack has been

    # memory
    def rd(self, a):
        v = self.on_read(a)
        return self.mem[a] if v is None else v

    def wr(self, a, v):
        self.on_write(a, v)
        self.mem[a] = v

    def push(self, v):
        self.mem[0x100 + self.sp] = v
        self.sp = (self.sp - 1) & 0xFF
        self.low_sp = min(self.low_sp, self.sp)

    def pull(self):
        self.sp = (self.sp + 1) & 0xFF
        return self.mem[0x100 + self.sp]

    def nz(self, v):
        self.p = (self.p & ~(N | Z)) | (v & N) | (0 if v else Z)
        return v

    def call(self, addr, a=0, x=0, y=0, limit=1000000):
        """JSR addr with these registers; returns the cycles it took."""
        self.a, self.x, self.y = a, x, y
        ret = SENTINEL - 1
        self.push(ret >> 8)
        self.push(ret & 0xFF)
        self.pc = addr
        cycles = 0
        while self.pc != SENTINEL:
            cycles += self.step()
            if cycles > limit:
                raise Stop("still running after %d cycles" % limit)
        return cycles

    def addr_of(self, mode):
        m, pc = self.mem, self.pc
        if mode == "zp":
            self.pc += 1
            return m[pc]
        if mode == "zpx":
            self.pc += 1
            return (m[pc] + self.x) & 0xFF
        if mode == "zpy":
            self.pc += 1
            return (m[pc] + self.y) & 0xFF
        if mode == "abs":
            self.pc += 2
            return m[pc] | m[pc + 1] << 8
        if mode == "abx":
            self.pc += 2
            return ((m[pc] | m[pc + 1] << 8) + self.x) & 0xFFFF
        if mode == "aby":
            self.pc += 2
            return ((m[pc] | m[pc + 1] << 8) + self.y) & 0xFFFF
        if mode == "izx":
            self.pc += 1
            z = (m[pc] + self.x) & 0xFF
            return m[z] | m[(z + 1) & 0xFF] << 8
        if mode == "izy":
            self.pc += 1
            z = m[pc]
            return ((m[z] | m[(z + 1) & 0xFF] << 8) + self.y) & 0xFFFF
        if mode == "ind":
            self.pc += 2
            p = m[pc] | m[pc + 1] << 8
            hi = (p & 0xFF00) | ((p + 1) & 0xFF)    # the page-wrap bug
            return m[p] | m[hi] << 8
        raise Stop("mode %s" % mode)

    def operand(self, mode):
        if mode == "imm":
            v = self.mem[self.pc]
            self.pc += 1
            return v
        return self.rd(self.addr_of(mode))

    def adc(self, v):
        a = self.a
        c = self.p & C
        if self.p & D:
            raise Stop("decimal mode at $%04X" % self.pc)
        r = a + v + c
        self.p &= ~(C | V)
        if r > 0xFF:
            self.p |= C
        if ~(a ^ v) & (a ^ r) & 0x80:
            self.p |= V
        self.a = self.nz(r & 0xFF)

    def cmp(self, reg, v):
        r = (reg - v) & 0x1FF
        self.p = (self.p & ~C) | (C if reg >= v else 0)
        self.nz(r & 0xFF)

    def rmw(self, mode, fn):
        if mode == "acc":
            self.a = self.nz(fn(self.a))
            return
        a = self.addr_of(mode)
        self.wr(a, self.nz(fn(self.rd(a))))

    def step(self):
        pc = self.pc
        self.on_fetch(pc)
        op = self.mem[pc]
        if op not in MODES:
            raise Stop("opcode $%02X at $%04X, which this check does not know" % (op, pc))
        name, mode, cyc = MODES[op]
        self.pc = (pc + 1) & 0xFFFF
        p = self.p

        if name == "LDA":
            self.a = self.nz(self.operand(mode))
        elif name == "LDX":
            self.x = self.nz(self.operand(mode))
        elif name == "LDY":
            self.y = self.nz(self.operand(mode))
        elif name == "STA":
            self.wr(self.addr_of(mode), self.a)
        elif name == "STX":
            self.wr(self.addr_of(mode), self.x)
        elif name == "STY":
            self.wr(self.addr_of(mode), self.y)
        elif mode == "rel":
            off = self.mem[pc + 1]
            self.pc = (pc + 2) & 0xFFFF
            take = {"BPL": not p & N, "BMI": p & N, "BVC": not p & V,
                    "BVS": p & V, "BCC": not p & C, "BCS": p & C,
                    "BNE": not p & Z, "BEQ": p & Z}[name]
            if take:
                self.pc = (self.pc + off - (256 if off & 0x80 else 0)) & 0xFFFF
                cyc += 1
        elif name in ("ADC", "SBC"):
            v = self.operand(mode)
            self.adc(v if name == "ADC" else v ^ 0xFF)
        elif name == "AND":
            self.a = self.nz(self.a & self.operand(mode))
        elif name == "ORA":
            self.a = self.nz(self.a | self.operand(mode))
        elif name == "EOR":
            self.a = self.nz(self.a ^ self.operand(mode))
        elif name == "CMP":
            self.cmp(self.a, self.operand(mode))
        elif name == "CPX":
            self.cmp(self.x, self.operand(mode))
        elif name == "CPY":
            self.cmp(self.y, self.operand(mode))
        elif name == "BIT":
            v = self.operand(mode)
            self.p = (p & ~(N | V | Z)) | (v & (N | V)) | (0 if v & self.a else Z)
        elif name in ("INC", "DEC"):
            d = 1 if name == "INC" else 0xFF
            self.rmw(mode, lambda v: (v + d) & 0xFF)
        elif name == "ASL":
            def f(v):
                self.p = (self.p & ~C) | (C if v & 0x80 else 0)
                return (v << 1) & 0xFF
            self.rmw(mode, f)
        elif name == "LSR":
            def f(v):
                self.p = (self.p & ~C) | (v & 1)
                return v >> 1
            self.rmw(mode, f)
        elif name == "ROL":
            def f(v):
                c = self.p & C
                self.p = (self.p & ~C) | (C if v & 0x80 else 0)
                return ((v << 1) | c) & 0xFF
            self.rmw(mode, f)
        elif name == "ROR":
            def f(v):
                c = self.p & C
                self.p = (self.p & ~C) | (v & 1)
                return (v >> 1) | (0x80 if c else 0)
            self.rmw(mode, f)
        elif name == "INX":
            self.x = self.nz((self.x + 1) & 0xFF)
        elif name == "INY":
            self.y = self.nz((self.y + 1) & 0xFF)
        elif name == "DEX":
            self.x = self.nz((self.x - 1) & 0xFF)
        elif name == "DEY":
            self.y = self.nz((self.y - 1) & 0xFF)
        elif name == "TAX":
            self.x = self.nz(self.a)
        elif name == "TAY":
            self.y = self.nz(self.a)
        elif name == "TXA":
            self.a = self.nz(self.x)
        elif name == "TYA":
            self.a = self.nz(self.y)
        elif name == "TSX":
            self.x = self.nz(self.sp)
        elif name == "TXS":
            self.sp = self.x
        elif name == "PHA":
            self.push(self.a)
        elif name == "PHP":
            self.push(p | B | U)
        elif name == "PLA":
            self.a = self.nz(self.pull())
        elif name == "PLP":
            self.p = (self.pull() & ~B) | U
        elif name == "JMP":
            self.pc = self.addr_of(mode)
        elif name == "JSR":
            target = self.addr_of("abs")
            ret = (self.pc - 1) & 0xFFFF
            self.push(ret >> 8)
            self.push(ret & 0xFF)
            self.pc = target
        elif name == "RTS":
            lo = self.pull()
            self.pc = ((self.pull() << 8 | lo) + 1) & 0xFFFF
        elif name in ("CLC", "SEC", "CLI", "SEI", "CLV", "CLD", "SED"):
            bit = {"C": C, "I": I, "V": V, "D": D}[name[2]]
            self.p = (p | bit) if name[0] == "S" else (p & ~bit)
        elif name == "NOP":
            pass
        else:   # BRK, RTI
            raise Stop("%s at $%04X" % (name, pc))
        return cyc
