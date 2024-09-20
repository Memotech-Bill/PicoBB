    .syntax unified
    adc     r0, #1
    adcs.w  r8, r9, #5
    adcs    r1, r2
    adcs    r3, r4, r5, rrx
    adc     r8, r8, r9, lsl #5
    adcs.w  r1, r9, r10, lsr #7
    adc.w   r1, r2, r3, asr #9
    add     r7, sp, #24
    add     sp, #96
    adds    r1, sp, #272
l001:   
    add     r8, sp, #0xFED
    add.n   r9, sp
    add.n   sp, r1
    adds    r3, sp, r4, ror #11
    adds    r5, r6, #5
    adds    r7, #13
    adds    r9, #17
    add     r10, r11, #19
    adds    r1, r3, r5
    adds    r6, r7
    adds    r1, r9, r4
    adr.n   r2, l002
    adr.w   r3, l003
    adr.w   r4, l001
    add     r8, r1
    add     r9, r3, r4
    and     r1, #13
    ands    r2, r3, #63
    ands    r4, r5
l002:   
    ands    r6, r6, r7
    and     r8, r9, r10
    ands    r2, r4, r6, ror #8
    asrs.w  r7, r6                  @ GCC will not generate narrow opcode
    asrs.w  r5, r5, r4              @ GCC will not generate narrow opcode
    asrs.w  r3, r2
    asr     r1, r5, r9
    asrs    r0, #13
    asr     r2, r8, #17
    b       l003
    bgt     l003
    b.w     l003
    blt     l001
    ble.w   l002
    bfc     r4, #6, #9
    bfi     r6, r6, #5, #3
    bfi     r10, r9, #7, #12
    bic     r3, #96
    bics    r5, r10, #0xA50
    bics    r2, r6
    bic     r0, r1, r2
    bkpt    #57
    bl      l003
    bl      l004
    blx     r3
    blxns   r7
    bx      r9
    bxns    r10
    cbnz    r0, l003
    clrex
    clz     r10, r6
l003:   
    cmn     r1, #0xA500
    cmn     r3, r2
    cmn     r7, r9
    cmn     r5, r4, lsl #7
    cmp     r6, #56
    cmp     r7, #0xC30
    cmp     r1, r0
    cmp     r3, sp
    cmp     r5, r7, lsr #9
    cpsie   fi
    cpsid   f
    csdb
l004:   
    dbg     3
    dmb
    dsb     sy
    eor     r1, #56
    eors    r5, r9, #0x340
    eors    r1, r3
    eors    r1, r2, r3
    eor     r8, r5, r4, asr #21
    isb     sy
    ittet   eq
    addeq   r0, r1, r2
    andeq   r3, r3, r4
    subne   r2, r3, #5
    beq     l004
    itet    ls
    eorls   r8, r9, #123
    orrhi   r8, r9, #45
    blls    l006
    lda     r2, [r4]
    ldab    r9, [r10]
    ldaex   r0, [r1]
    ldaexb  r2, [r3]
    ldaexh  r4, [r5]
    ldah    r6, [r7]
    ldm     r0!, {r1-r3, r5}
    ldm     r1, {r1, r4, r5}
    ldmia   r2!, {r3-r5, r7}
    ldmfd   r3, {r0, r5-r7}
    ldm     r4, {r2, r6, r8, pc}
    ldmia   r5!, {r0, r6-r9, lr}
    ldmdb   sp!, {r5-r7}
    ldmea   r9, {r0, r3, r6}
    ldr     r1, [r3, #12]
    ldr     r2, [sp, #48]
    ldr     r8, [r0, #360]
    ldr     r9, [r1, #-20]
    ldr     r7, [r2], #39
    ldr     r6, [r3, #-51]!
    ldr     r5, [r4, r0]
    ldr     r4, [r3, r1, lsl #2]
    ldr     r10, [r5, r6]
    ldr.w   r0, l005
    ldr.w   r3, l004
    ldrb    r1, [r3, #12]
    ldrb    r2, [sp, #48]
l005:   
    ldrb    r8, [r0, #360]
    ldrb    r9, [r1, #-20]
    ldrb    r7, [r2], #39
    ldrb    r6, [r3, #-51]!
    ldrb    r5, [r4, r0]
    ldrb    r4, [r3, r1, lsl #1]
    ldrb    r10, [r5, r6]
    ldrb    r4, l005
    ldrbt   r9, [r8, #28]
    ldrd    r0, r1, [r8]
    ldrd    r2, r3, [r9, #-28]
    ldrd    r4, r5, [r10], #-12
    ldrd    r6, r7, [r11, #24]!
.balign 4
    ldrd    r5, r7, l006
    ldrex   r8, [r0, #92]
    ldrexb  r9, [r1]
    ldrexh  r10, [r2]
l006:   
    ldrh    r2, [r4, #10]
    ldrh    r3, [r5, #740]
    ldrh    r4, [r6, #-6]
    ldrh    r5, [r7], #13
    ldrh    r6, [r8, #-17]!
    ldrh    r6, l006
    ldrht   r7, [r9, #24]
    ldrsb   r0, [r1, #512]
    ldrsb   r2, [r3]
    ldrsb   r4, [r5]!
    ldrsb   r7, [r6, r5, lsl #3]
    ldrsb   r8, l007
    ldrsbt  r0, [r1]
    ldrsh   r8, [r9, #1040]
    ldrsh   r10, [r11, #3]
    ldrsh   r7, [r6], #-7
    ldrsh   r5, [r4, #-9]!
    ldrsh   r3, [r2, r1]
    ldrsh   r1, l007
    ldrsht  r10, [r6]
l007:   
    ldrt    r9, [r5, #15]
    lsls    r0, r1, #8
    lsl     r0, r1, #8
    lsls    r2, r3
    lsls.w  r4, r5
    lsl     r6, r7, r8
    lsrs    r7, r6, #24
    lsr     r5, r4, #20
    lsrs    r3, r2, r1
    lsrs.w  r0, r1
    mla     r0, r1, r2, r3
    mls     r4, r5, r6, r7
    movs    r1, #13
    mov     r2, #0xA5000000
    mov     r3, #0x1234
    mov     r11, r3
    movs    r4, r6, asr #27
    mov     r8, r4, lsl #5
    movs    r10, r12, rrx
    movt    r4, #0x5678
    mrs     r0, msp
    msr     apsr_g, r1
    muls    r2, r3
    mvn     r4, #133
    mvns    r5, #0xA5A5A5A5
    nop
    nop.w
    orn     r6, #211
    orns    r7, r8, r9, rrx
    orn     r10, r11, r12, lsl #8
    orrs    r0, r3, #101
    orr     r1, r4, r7, asr #16
    pkhbt   r2, r3, r4, lsl #6
    pkhtb   r5, r6, r7, asr #10
    pld     [r0, #175]
    pld     [r1, #1040]
    pld     [r2, r3, lsl #3]
    pld     [r4, r5]
    pld     l008
    pli     [r6, #43]
    pli     [r7, #-67]
    pli     [r8, r9, lsl #1]
    pop     {r2-r6, pc}
    pop     {r1, r3, r7}
    pop     {r4-r10, lr}
    pop     {r3, r6, r9, pc}
    pop     {r5}
    pop     {r10}
    pssbb
    push    {r1, r3, r5, lr}
    push    {r4-r7}
    push    {r8-r10, lr}
    push    {r1, r11, lr}
    push    {r4}
    push    {r9}
l008:   
    qadd    r0, r4, r8
    qadd16  r1, r5, r9
    qadd8   r2, r6, r10
    qasx    r3, r7, r11
    qdadd   r4, r8, r12
    qdsub   r5, r9, r1
    qsax    r6, r10, r2
    qsub    r7, r11, r3
    qsub16  r8, r12, r4
    qsub8   r9, r1, r5
    rbit    r10, r2
    rev     r0, r1
    rev     r11, r3
    rev16   r2, r3
    rev16   r12, r4
    revsh   r4, r5
    revsh   r1, r9
    ror     r2, #17
    rors    r3, r4, #21
    rors    r5, r5, r6
    ror     r7, r8, r9
    rrx     r10, r9
    rrxs    r11, r12
    rsbs    r2, r4, #0
    rsb     r6, r8, #59
    rsb     r3, r2, r1, rrx
    rsbs    r6, r5, r4, ror #8
    sadd16  r0, r1, r2
    sadd8   r1, r2, r3
    sasx    r2, r3, r4
    sbc     r3, #0x4C004C
    sbcs    r4, r5, #0x17001700
    sbcs    r5, r6, r7
    sbc     r6, r7, r8
    sbcs    r7, r8, r9, lsl #3
    sbfx    r8, r9, #16, #8
    sdiv    r9, r10, r11
    sel     r12, r11, r10
    sev
    sev.w
    sg
    shadd16 r11, r10, r9
    shadd8  r10, r9, r8
    shasx   r9, r8, r7
    shsax   r8, r7, r6
    shsub16 r7, r6, r5
    shsub8  r6, r5, r4
    smlabb  r0, r2, r4, r6
    smlabt  r1, r3, r5, r7
    smlatb  r2, r4, r6, r8
    smlatt  r3, r5, r7, r9
    smlad   r4, r6, r8, r10
    smladx  r5, r7, r9, r11
    smlal   r6, r8, r10, r12
    smlalbb r3, r2, r1, r0
    smlalbt r4, r3, r3, r1
    smlaltb r5, r4, r3, r2
    smlaltt r6, r5, r4, r4
    smlald  r7, r6, r5, r4
    smlaldx r8, r7, r6, r5
    smlawb  r9, r8, r7, r6
    smlawt  r10, r9, r8, r7
    smlsd   r11, r10, r9, r8
    smlsdx  r12, r11, r10, r9
    smlsld  r0, r2, r4, r6
    smlsldx r1, r3, r5, r7
    smmla   r2, r4, r6, r8
    smmlar  r3, r5, r7, r9
    smmls   r4, r6, r8, r10
    smmlsr  r5, r7, r9, r11
    smuad   r6, r8, r10
    smuadx  r7, r9, r11
    smulbb  r11, r9, r7
    smulbt  r10, r8, r6
    smultb  r9, r7, r5
    smultt  r8, r6, r4
    smull   r7, r5, r3, r1
    smulwb  r6, r4, r2
    smulwt  r5, r3, r1
    smusd   r4, r2, r0
    smusdx  r0, r1, r2
    ssat    r1, #16, r2, asr #4
    ssat    r2, #24, r3, lsl #6
    ssat16  r3, #12, r4
    ssax    r4, r5, r6
    ssbb
    ssub16  r5, r6, r7
    ssub8   r6, r7, r8
    stl     r7, [r8]
    stlb    r8, [r9]
    stlex   r9, r10, [r11]
    stlexb  r10, r11, [r12]
    stlexh  r12, r11, [r10]
    stlh    r11, [r10]
    stm     r7!, {r1, r3, r5}
    stmia   r6!, {r2-r4}
    stmea   r5!, {r1-r3, r7}
    stm     sp!, {r4-r7, r9, lr}
    stmia   r0, {r0-r6, r10}
    stmea   r8, {r4-r7}
    stmdb.w sp!, {r0-r3, r6}                @ GCC will not generate narrow opcode
    stmfd   r8, {r4-r9}
    stmdb   r0!, {r4, r6, r8, r10}
    str     r0, [r1, #24]
    str     r1, [sp, #188]
    str.w   r2, [r3, #1045]
    str     r3, [r4, #-76]
    str     r4, [r5, r6]
    str     r5, [r6, r7, lsl #2]
    strb    r0, [r2, #81]
    strb.w  r1, [r3, #876]
    strb    r2, [r4, #-155]
    strb    r3, [r5, r7]
    strb    r4, [r6, r8, lsl #1]
    strbt   r5, [r7, #37]
    strd    r0, r1, [r2, #-44]
    strd    r1, r3, [r5], #56
    strd    r2, r5, [r8, #-72]!
    strex   r12, r10, [r8, #28]
    strexb  r10, r8, [r6]
    strexh  r8, r6, [r4]
    strh    r7, [r6, #14]
    strh    r6, [r5, #4000]
    strh    r5, [r4, #-100]
    strh    r4, [r3], #52
    strh    r3, [r2, #-16]!
    strht   r2, [r1, #212]
    strt    r1, [r0, #84]
    sub     sp, #16
    sub     r0, sp, #192
    subs    r1, sp, #0x120
    sub     r3, sp, r4, lsl #2
    subs    r4, sp, r5, rrx
    subs    r6, r7, #5
    subs    r7, r7, #254
    sub     r8, r9, #0xbe00
    subw    r10, pc, #0x468
    subs    r7, r6, r5
    sub     r6, r5, r4, ror #6
    svc     #13
    sxtab   r7, r6, r5, ror #24
    sxtab16 r8, r7, r6, ror #8
    sxtah   r9, r8, r7, ror #16
    sxtb    r7, r6
    sxtb    r10, r9, ror #8
    sxtb16  r11, r10, ror #16
    sxth    r6, r5
    sxth    r12, r11, ror #24
    tbb     [r11, r10]
    tbh     [r10, r9, lsl #1]
    teq     r9, #123
    teq     r8, r7, ror #4
    tst     r7, #98
    tst     r6, r5, lsr #12
    tt      r5, r4
    tta     r4, r3
    ttat    r3, r2
    ttt     r2, r1
    uadd16  r0, r1, r2
    uadd8   r1, r2, r3
    uasx    r2, r3, r4
    ubfx    r3, r4, #6, #12
    udf     #1
    udf.w   #2
    udiv    r4, r5, r6
    uhadd16 r5, r6, r7
    uhadd8  r6, r7, r8
    uhasx   r7, r8, r9
    uhsax   r8, r9, r10
    uhsub16 r9, r10, r11
    uhsub8  r10, r11, r12
    umaal   r3, r2, r1, r0
    umlal   r4, r3, r2, r1
    umull   r5, r4, r3, r2
    uqadd16 r6, r5, r4
    uqadd8  r7, r6, r5
    uqasx   r8, r7, r6
    uqsax   r9, r8, r7
    uqsub16 r10, r9, r8
    uqsub8  r11, r10, r9
    usad8   r12, r11, r10
    usada8  r9, r10, r11, r12
    usat    r8, #28, r9, lsl #4
    usat16  r7, #12, r8
    usax    r6, r7, r8
    usub16  r5, r6, r7
    usub8   r4, r5, r6
    uxtab   r3, r4, r5, ror #16
    uxtab16 r2, r3, r4, ror #8
    uxtah   r1, r2, r3, ror #24
    uxtb    r0, r1
    uxtb    r1, r0, ror #16
    uxtb16  r2, r1, ror #8
    uxth    r3, r2
    uxth    r4, r3, ror #24
    vabs.f32    s0, s1
    vadd.f32    s1, s2, s3
    vcmp.f32    s2, s3
    vcmpe.f32   s3, s4
    vcvt.f32.s32    s4, s5
    vcvt.u32.f32    s5, s6
    vcvt.f32.u32    s6, s6, #16
    vcvt.s32.f32    s7, s7, #20
    vcvta.s32.f32   s8, s9
    vcvtm.u32.f32   s9, s10
    vcvtn.s32.f32   s10, s11
    vcvtp.u32.f32   s11, s12
    vcvtr.s32.f32   s12, s13
    vdiv.f32    s17, s18, s19
    vfma.f32    s18, s19, s20
    vfms.f32    s19, s20, s21
    vfnma.f32   s20, s21, s22
    vfnms.f32   s21, s22, s23
l009:   
    vldm.f32    r0, {s3-s8}
    vldmia.f32  r1!, {s7-s11}
    vldmdb.f32  r2!, {s15-s20}
    vldr.f32    s22, [r3, #36]
    vldr.f32    s0, l009
    vlldm   r4
    vlstm   r5
    vmaxnm.f32  s23, s24, s25
    vminnm.f32  s24, s25, s26
    vmla.f32    s25, s26, s27
    vmls.f32    s26, s27, s28
    vmov.f32    r6, s27
    vmov.f32    s28, r7
    vmov.f32    r8, r9, s29, s30
    vmov.f32    s30, s31, r9, r10
    vmov.f32    s31, #2.5
    vmov.f32    s0, s2
    vmrs    r10, fpscr
    vmsr    fpscr, r11
    vmul.f32    s1, s3, s5
    vneg.f32    s2, s4
    vnmla.f32   s3, s5, s7
    vnmls.f32   s4, s6, s8
    vnmul.f32   s5, s7, s9
    vpop.f32    {s5-s8}
    vpush.f32   {s10-s20}
    vrinta.f32  s6, s8
    vrintm.f32  s7, s9
    vrintn.f32  s8, s10
    vrintp.f32  s9, s11
    vrintr.f32  s10, s12
    vrintx.f32  s11, s13
    vrintz.f32  s12, s14
    vseleq.f32  s14, s16, s18
    vselge.f32  s15, s17, s19
    vselgt.f32  s16, s18, s20
    vselvs.f32  s17, s19, s21
    vsqrt.f32   s18, s20
    vstm.f32    r10, {s19-s21}
    vstmia.f32  r11!, {s20-s22}
    vstmdb.f32  r12!, {s21-s23}
    vstr.f32    s22, [r0, #68]
    vsub.f32    s23, s25, s27
    wfe
    wfe.w
    wfi
    wfi.w
    yield
    yield.w
    
