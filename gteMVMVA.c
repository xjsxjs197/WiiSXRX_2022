#ifdef _IN_GTE

#define _MVMVA_LM(lm) \
    gteMAC1 = A1(SSX); \
    gteMAC2 = A1(SSY); \
    gteMAC3 = A1(SSZ); \
    gteIR1 = limB1(gteMAC1, lm); \
    gteIR2 = limB2(gteMAC2, lm); \
    gteIR3 = limB3(gteMAC3, lm);

void gteMVMVA_V0_R_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_R_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_R_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_R_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_R_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_R_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_V2_R_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_IR_R_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_V0_L_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_V2_L_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_IR_L_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_V0_C_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_V2_C_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_IR_C_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteTRX << 12;
    SSY += (s64)gteTRY << 12;
    SSZ += (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRBK << 12;
    SSY += (s64)gteGBK << 12;
    SSZ += (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX += (s64)gteRFC << 12;
    SSY += (s64)gteGFC << 12;
    SSZ += (s64)gteBFC << 12;

    _MVMVA_LM(0);
}


void gteMVMVA_TR_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteTRX << 12;
    SSY = (s64)gteTRY << 12;
    SSZ = (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_TR_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteTRX << 12;
    SSY = (s64)gteTRY << 12;
    SSZ = (s64)gteTRZ << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_TR_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteTRX << 12;
    SSY = (s64)gteTRY << 12;
    SSZ = (s64)gteTRZ << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_TR_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteTRX << 12;
    SSY = (s64)gteTRY << 12;
    SSZ = (s64)gteTRZ << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_BK_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRBK << 12;
    SSY = (s64)gteGBK << 12;
    SSZ = (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_BK_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRBK << 12;
    SSY = (s64)gteGBK << 12;
    SSZ = (s64)gteBBK << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_BK_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRBK << 12;
    SSY = (s64)gteGBK << 12;
    SSZ = (s64)gteBBK << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_BK_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRBK << 12;
    SSY = (s64)gteGBK << 12;
    SSZ = (s64)gteBBK << 12;

    _MVMVA_LM(0);
}

void gteMVMVA_FC_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRFC << 12;
    SSY = (s64)gteGFC << 12;
    SSZ = (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_FC_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRFC << 12;
    SSY = (s64)gteGFC << 12;
    SSZ = (s64)gteBFC << 12;

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_FC_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRFC << 12;
    SSY = (s64)gteGFC << 12;
    SSZ = (s64)gteBFC << 12;

    _MVMVA_LM(1);
}

void gteMVMVA_FC_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    SSX = (s64)gteRFC << 12;
    SSY = (s64)gteGFC << 12;
    SSZ = (s64)gteBFC << 12;

    _MVMVA_LM(0);
}



void gteMVMVA_V0_R_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_R_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    _MVMVA_LM(1);
}

void gteMVMVA_V0_R_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteR);

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_R_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    _MVMVA_LM(1);
}

void gteMVMVA_V1_R_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteR);

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_R_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    _MVMVA_LM(1);
}

void gteMVMVA_V2_R_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteR);

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_R_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    _MVMVA_LM(1);
}

void gteMVMVA_IR_R_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteR);

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_L_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    _MVMVA_LM(1);
}

void gteMVMVA_V0_L_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gteL);

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_L_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    _MVMVA_LM(1);
}

void gteMVMVA_V1_L_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gteL);

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_L_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    _MVMVA_LM(1);
}

void gteMVMVA_V2_L_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gteL);

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_L_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    _MVMVA_LM(1);
}

void gteMVMVA_IR_L_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gteL);

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V0_C_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    _MVMVA_LM(1);
}

void gteMVMVA_V0_C_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX0, gteVY0, gteVZ0, gte_C);

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V1_C_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    _MVMVA_LM(1);
}

void gteMVMVA_V1_C_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX1, gteVY1, gteVZ1, gte_C);

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_V2_C_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    _MVMVA_LM(1);
}

void gteMVMVA_V2_C_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteVX2, gteVY2, gteVZ2, gte_C);

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    SSX >>= 12; SSY >>= 12; SSZ >>= 12;

    _MVMVA_LM(0);
}

void gteMVMVA_IR_C_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    _MVMVA_LM(1);
}

void gteMVMVA_IR_C_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    _MVMVA_FUNC(gteIR1, gteIR2, gteIR3, gte_C);

    _MVMVA_LM(0);
}

void gteMVMVA_SF1_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    gteMAC1 = 0;
    gteMAC2 = 0;
    gteMAC3 = 0;
    gteIR1 = 0;
    gteIR2 = 0;
    gteIR3 = 0;
}

void gteMVMVA_SF1_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    gteMAC1 = 0;
    gteMAC2 = 0;
    gteMAC3 = 0;
    gteIR1 = 0;
    gteIR2 = 0;
    gteIR3 = 0;
}

void gteMVMVA_SF0_LM1(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    gteMAC1 = 0;
    gteMAC2 = 0;
    gteMAC3 = 0;
    gteIR1 = 0;
    gteIR2 = 0;
    gteIR3 = 0;
}

void gteMVMVA_SF0_LM0(psxCP2Regs *regs) {
    s64 SSX, SSY, SSZ;
    gteFLAG = 0;

    gteMAC1 = 0;
    gteMAC2 = 0;
    gteMAC3 = 0;
    gteIR1 = 0;
    gteIR2 = 0;
    gteIR3 = 0;
}

#endif
