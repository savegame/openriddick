
void __thiscall FUN_102a9f00(void *this,int param_1)

{
  ushort uVar1;
  bool bVar2;
  char cVar3;
  byte bVar4;
  uint uVar5;
  float fVar6;
  int iVar7;
  undefined4 uVar8;
  int *piVar9;
  void *this_00;
  ushort uVar10;
  undefined4 extraout_ECX;
  undefined4 extraout_EDX;
  ulonglong uVar11;
  int iVar12;
  int local_104;
  float local_100 [4];
  float local_f0;
  float local_ec;
  float local_e0;
  float local_dc;
  undefined4 local_d8;
  int iStack_d4;
  float local_d0;
  float local_cc;
  float local_c8;
  float local_c4;
  float local_c0;
  float local_bc;
  uint auStack_a0 [9];
  undefined2 uStack_7c;
  void *pvStack_24;
  void *pvStack_1c;
  undefined1 *puStack_18;
  undefined4 uStack_14;
  
  uStack_14 = 0xffffffff;
  puStack_18 = &LAB_1070b93b;
  pvStack_1c = ExceptionList;
  ExceptionList = &pvStack_1c;
  uVar5 = FUN_102bea60((int)this);
  if (uVar5 != 3) {
    uVar5 = FUN_102bea60((int)this);
    if ((uVar5 != 4) && (uVar5 = FUN_102bea60((int)this), uVar5 != 5)) {
      uVar1 = *(ushort *)(param_1 + 0x2a8c);
      if (((uVar1 & 1) == 0) &&
         (((*(byte *)((int)this + 0x19c) & 1) == 0 && ((*(byte *)((int)this + 0x178) & 0x90) == 0)))
         ) {
        bVar2 = false;
      }
      else {
        bVar2 = true;
      }
      local_104._0_1_ = !bVar2;
      if (((*(uint *)(param_1 + 0x1c0) & 0x40000000) == 0) && ((uVar1 & 0x102) == 0)) {
        uVar10 = *(ushort *)((int)this + 0x17a);
        if (((uVar10 & 0x4000) == 0) && ((*(uint *)((int)this + 0x19c) & 0x10000000) == 0)) {
LAB_102a9fd2:
          local_104._0_1_ = true;
        }
      }
      else {
        uVar10 = *(ushort *)((int)this + 0x17a);
        if ((uVar10 & 0x4000) != 0) goto LAB_102a9fd2;
      }
      if (((*(uint *)(param_1 + 0x1c0) & 0x10) == 0) && ((uVar1 & 1) == 0)) {
        if (((uVar10 & 0x90) == 0) && ((*(uint *)((int)this + 0x19c) & 0x10000000) == 0)) {
LAB_102a9ff9:
          local_104._0_1_ = true;
        }
      }
      else if ((uVar10 & 0x90) != 0) goto LAB_102a9ff9;
      if (((*(uint *)(param_1 + 0x2b78) & 0x400) != 0) &&
         (uVar5 = FUN_102bea60((int)this), uVar5 != 3)) {
        if (((*(byte *)(param_1 + 0x2bc0) & 4) == 0) && ((*(byte *)(param_1 + 0x2a8c) & 2) == 0)) {
          if ((!(bool)(char)local_104) && ((*(ushort *)((int)this + 0x17a) & 0x800) != 0)) {
LAB_102aa0f3:
            local_104._0_1_ = false;
            goto LAB_102aa04b;
          }
        }
        else if ((!(bool)(char)local_104) && ((*(ushort *)((int)this + 0x17a) & 0x800) == 0))
        goto LAB_102aa0f3;
        local_104._0_1_ = true;
      }
LAB_102aa04b:
      if (((*(uint *)(param_1 + 0x1c0) & 0x80000) != 0) &&
         ((*(short *)((int)this + 0x17a) != 0 ||
          (local_104._0_1_ = false, *(short *)((int)this + 0x178) != 0)))) {
        local_104._0_1_ = true;
      }
      if (*(char *)((int)this + 0x58d) == '\0') {
LAB_102aa08d:
        iVar12 = *(int *)((int)this + 0x200);
        if (iVar12 != 0) {
LAB_102aa097:
          local_104._0_1_ = true;
          *(bool *)((int)this + 0x58d) = iVar12 != 0;
        }
      }
      else {
        iVar12 = 0;
        if (*(int *)((int)this + 0x200) == 0) goto LAB_102aa097;
        if (*(char *)((int)this + 0x58d) == '\0') goto LAB_102aa08d;
      }
      bVar2 = FUN_102c2b20((int)this);
      if (bVar2) {
        piVar9 = *(int **)((int)this + 0x2b0);
        cVar3 = '\x01';
        iVar12 = 0;
        uVar5 = FUN_102bea60((int)this);
        fVar6 = (float)(2 - (uint)(uVar5 != 2));
      }
      else {
        if (((*(byte *)(param_1 + 0x4e4) & 8) == 0) ||
           (fVar6 = 2.8026e-45, (*(uint *)((int)this + 0x19c) & 0x40000000) != 0)) {
          fVar6 = 1.4013e-45;
        }
        uVar5 = FUN_102bea20((int)this);
        if ((uVar5 == 1) ||
           ((uVar5 = FUN_102bea20((int)this), uVar5 == 0xc &&
            ((*(uint *)(param_1 + 0x1c0) & 0x20000) != 0)))) {
          fVar6 = 2.8026e-45;
        }
        else if ((*(uint *)(param_1 + 0x1c0) & 0x100000) != 0) {
          fVar6 = 1.4013e-45;
        }
        if ((*(byte *)(param_1 + 0x2074) & 2) != 0) {
          *(uint *)(param_1 + 0x4e4) = *(uint *)(param_1 + 0x4e4) & 0xfffffff7;
          fVar6 = 1.4013e-45;
        }
        piVar9 = *(int **)((int)this + 0x2b0);
        cVar3 = '\x01';
        iVar12 = 0;
      }
      FUN_102f0fa0(this,piVar9,(float *)piVar9,fVar6,iVar12,(char)local_104,cVar3);
      if ((*(uint *)(param_1 + 0x1c0) & 0x400000) == 0) {
        if ((*(uint *)((int)this + 0x304) & 0x40000000) != 0) {
          (**(code **)(*(int *)this + 0x84))(0,0);
        }
      }
      else if ((*(uint *)((int)this + 0x304) & 0x40000000) == 0) {
        FUN_103b8820(this,&local_d0);
        local_100[3] = ABS(*(float *)(param_1 + 0x19c));
        local_100[0] = -0.0 - local_100[3];
        local_100[2] = 0.0;
        local_d8 = 0;
        local_ec = 0.0;
        local_100[1] = local_100[0];
        local_f0 = local_100[3];
        local_e0 = local_100[3];
        local_dc = local_100[3];
        SSE_Box3Dfp32_Expand((TBox<float> *)local_100,(TBox<float> *)&local_d0);
        *(uint *)((int)this + 0x304) = *(uint *)((int)this + 0x304) | 0x40000000;
        if (((((local_c4 != local_100[3]) || (local_c0 != local_f0)) || (local_bc != local_ec)) ||
            ((local_d0 != local_100[0] || (local_cc != local_100[1])))) ||
           (local_c8 != local_100[2])) {
          (**(code **)(**(int **)((int)this + 0x2b0) + 0x200))
                    ((int)*(short *)((int)this + 0x1a0),local_100,local_100 + 3);
        }
      }
    }
    uVar8 = *(undefined4 *)(param_1 + 0x30);
    *(undefined4 *)(param_1 + 0x30) = 0;
    (**(code **)(*(int *)this + 0x178))();
    *(undefined4 *)(param_1 + 0x30) = uVar8;
  }
  if (param_1 != 0) {
    cVar3 = FUN_1029c490((int)this);
    if (((cVar3 == '\0') && (*(char *)(param_1 + 0x200c) != '\x02')) &&
       (*(char *)(param_1 + 0x200c) != '\x03')) {
      FUN_1029f670((int)this);
    }
    cVar3 = *(char *)(param_1 + 0x200c);
    if ((cVar3 != '\x02') && (cVar3 != '\0')) {
      iStack_d4 = *(int *)(param_1 + 0x28) - *(int *)(param_1 + 0x2010);
      fVar6 = (float)iStack_d4;
      if (iStack_d4 < 0) {
        fVar6 = fVar6 + 4.2949673e+09;
      }
      if (1.0 <= fVar6 / (*(float *)(param_1 + 0x2014) *
                         *(float *)(*(int *)((int)this + 0x2b0) + 0x2a0))) {
        if (cVar3 == '\x01') {
          if (*(char *)(param_1 + 0x200c) != '\0') {
            *(undefined1 *)(param_1 + 0x200c) = 0;
            goto LAB_102aa3db;
          }
        }
        else if ((cVar3 == '\x03') && (*(char *)(param_1 + 0x200c) != '\x02')) {
          *(undefined1 *)(param_1 + 0x200c) = 2;
LAB_102aa3db:
          *(uint *)(param_1 + 0x1f74) = *(uint *)(param_1 + 0x1f74) | 0x20;
        }
      }
    }
  }
  FUN_1038c1a0(this,*(undefined4 *)((int)this + 0x2b0),param_1);
  uVar11 = FUN_10685660(extraout_ECX,extraout_EDX);
  *(short *)((int)this + 0x1f0) = (short)uVar11;
  bVar2 = FUN_102c2b20((int)this);
  if (!bVar2) {
    FUN_102ce970((int)this,*(undefined4 *)((int)this + 0x2b0));
  }
  uVar5 = FUN_102bea60((int)this);
  if (uVar5 == 3) {
    local_100[0] = *(float *)((int)this + 0xd0);
    local_100[1] = *(float *)((int)this + 0xd4);
    local_100[2] = *(float *)((int)this + 0xd8);
    local_100[3] = *(float *)((int)this + 0xdc) * 0.9;
    if (local_100[3] <= 0.01) {
      fVar6 = -0.01;
      if (local_100[3] < -0.01) goto LAB_102aa4a1;
    }
    else {
      fVar6 = 0.01;
LAB_102aa4a1:
      local_100[3] = fVar6;
    }
    (**(code **)(**(int **)((int)this + 0x2b0) + 0x210))
              ((int)*(short *)((int)this + 0x1a0),local_100);
  }
  else {
    *(uint *)((int)this + 0x2a8) = *(uint *)((int)this + 0x2a8) | 1;
  }
  iVar12 = (**(code **)(*(int *)this + 0x17c))();
  if (iVar12 == 0) {
    iVar7 = FUN_102b3dd0((int)this);
    iVar12 = *(int *)(iVar7 + 0xc);
    *(undefined4 *)(iVar7 + 0xc) = 0;
    *(uint *)((int)this + 0x2a8) = *(uint *)((int)this + 0x2a8) | iVar12 << 0x10;
    uVar8 = *(undefined4 *)(iVar7 + 0x1f74);
    *(undefined4 *)(iVar7 + 0x1f74) = 0;
    *(ushort *)((int)this + 0x2a6) = *(ushort *)((int)this + 0x2a6) | (ushort)uVar8;
    ExceptionList = pvStack_1c;
    return;
  }
  *(ushort *)(param_1 + 0x55c) = *(ushort *)(param_1 + 0x55c) & 0xfc01 | 1;
  FUN_102dfbc0(this);
  if (((uint)(*(float ********)((int)this + 0x2b0))[0xab] & 1) == 0) {
    FUN_102d0d90(this,*(float ********)((int)this + 0x2b0),(int *)((int)this + 0x50),
                 (float ******)0x0,(float *)0x0);
  }
  FUN_102b4180(this,'\x01');
  if (*(short *)(param_1 + 0x2d16) == -1) goto LAB_102aa7f9;
  if ((*(uint *)((int)this + 0x19c) & 0x8000000) != 0) {
    if ((*(int *)(param_1 + 0xc78) == 0) || (*(int *)(param_1 + 0xc78) == 0x2a0d0)) {
      iVar7 = (int)*(short *)(param_1 + 0x2000);
      local_e0 = 0.0;
      local_dc = 0.0;
      local_d8 = 0;
      local_100[0] = 0.0;
      local_100[1] = 0.0;
      local_100[2] = 0.0;
      iVar12 = **(int **)((int)this + 0x2b0);
      uVar8 = FUN_103b87b0(&local_d0,0x4041,0,0,0xffff,0,local_100,&local_e0,0,0);
      iVar12 = (**(code **)(iVar12 + 0x4d4))(uVar8,iVar7);
      if ((iVar12 != 0) || ((int)*(float *)(param_1 + 0x2528) == 0x1d)) {
        FUN_103b7210(this,*(uint *)((int)this + 0x19c) & 0xf7ffffff);
        goto LAB_102aa6db;
      }
      *(uint *)(param_1 + 0x4e4) = *(uint *)(param_1 + 0x4e4) & 0xfffffff7;
      FUN_103b7210(this,*(uint *)((int)this + 0x19c) & 0xb79fffff);
      bVar4 = *(byte *)(param_1 + 0x1ffc) & 0x7f;
      if (*(byte *)(param_1 + 0x1ffc) != bVar4) {
        *(uint *)(param_1 + 0x1f74) = *(uint *)(param_1 + 0x1f74) | 0x10;
        *(byte *)(param_1 + 0x1ffc) = bVar4;
      }
      bVar4 = *(byte *)(param_1 + 0x1ffc) & 0xf7;
      if (*(byte *)(param_1 + 0x1ffc) != bVar4) {
        *(uint *)(param_1 + 0x1f74) = *(uint *)(param_1 + 0x1f74) | 0x10;
        *(byte *)(param_1 + 0x1ffc) = bVar4;
      }
      bVar4 = *(byte *)((int)this + 0x5e7) | *(byte *)(param_1 + 0x1ffc);
    }
    else {
      *(uint *)(param_1 + 0x4e4) = *(uint *)(param_1 + 0x4e4) & 0xfffffff7;
      FUN_103b7210(this,*(uint *)((int)this + 0x19c) | 0x48600000);
      bVar4 = *(byte *)(param_1 + 0x1ffc) | 0x80;
      if (*(byte *)(param_1 + 0x1ffc) != bVar4) {
        *(uint *)(param_1 + 0x1f74) = *(uint *)(param_1 + 0x1f74) | 0x10;
        *(byte *)(param_1 + 0x1ffc) = bVar4;
      }
      bVar4 = *(byte *)(param_1 + 0x1ffc) | 8;
    }
    if (*(byte *)(param_1 + 0x1ffc) != bVar4) {
      *(uint *)(param_1 + 0x1f74) = *(uint *)(param_1 + 0x1f74) | 0x10;
      *(byte *)(param_1 + 0x1ffc) = bVar4;
    }
  }
LAB_102aa6db:
  if (((*(byte *)(param_1 + 0x2074) & 2) != 0) &&
     (bVar4 = *(byte *)(param_1 + 0x1ffc) | 0x80, *(byte *)(param_1 + 0x1ffc) != bVar4)) {
    *(uint *)(param_1 + 0x1f74) = *(uint *)(param_1 + 0x1f74) | 0x10;
    *(byte *)(param_1 + 0x1ffc) = bVar4;
  }
  uVar1 = *(ushort *)(param_1 + 0x21b4);
  if (uVar1 != 0) {
    local_100[2] = *(float *)(param_1 + 0xcb4);
    local_104 = 2;
    local_100[0] = (float)(int)*(short *)((int)this + 0x1a0);
    local_100[1] = (float)(uint)uVar1;
    if (((local_100[2] != 0.0) && (local_100[2] != (float)(uint)uVar1)) &&
       (local_100[2] != (float)(int)*(short *)((int)this + 0x1a0))) {
      local_104 = 3;
    }
    iVar12 = 0;
    if (local_104 != 0) {
      do {
        piVar9 = (int *)(**(code **)(**(int **)((int)this + 0x2b0) + 0x460))(local_100[iVar12]);
        this_00 = (void *)FUN_106866a8(piVar9,0,(_s_RTTICompleteObjectLocator *)
                                                &CWObject::RTTI_Type_Descriptor,
                                       &CWObject_Character::RTTI_Type_Descriptor,0);
        if (this_00 != (void *)0x0) {
          iStack_d4 = FUN_102b3dd0((int)this_00);
          if ((iStack_d4 != 0) &&
             ((uVar5 = FUN_102bea60((int)this_00), uVar5 == 3 ||
              (**(char **)(iStack_d4 + 0x84) != '\0')))) {
            FUN_102ea2e0(this);
            FUN_102eb410(this_00,'\0');
            break;
          }
        }
        iVar12 = iVar12 + 1;
      } while (iVar12 < local_104);
    }
  }
  if (((*(byte *)((int)this + 0x304) & 0x10) != 0) && (uVar5 = FUN_102bea60((int)this), uVar5 == 3))
  {
    FUN_102c17b0(this);
  }
LAB_102aa7f9:
  pvStack_24 = (void *)0x0;
  auStack_a0[0] = 0;
  uStack_7c = 0;
  uStack_14 = 0;
  FUN_102eaa90((int)this,*(int *)((int)this + 0x2b0),0,0,auStack_a0);
  FUN_102ebe50(this,auStack_a0);
  FUN_102eb720(this);
  uVar5 = FUN_102bea60((int)this);
  if (uVar5 != 3) {
    FUN_102ceb80((int)this);
  }
  if (pvStack_24 != (void *)0x0) {
    FUN_10012b80(pvStack_24);
  }
  ExceptionList = pvStack_1c;
  return;
}

