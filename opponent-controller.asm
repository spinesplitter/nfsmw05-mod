
t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\speed.exe:     file format pei-i386


Disassembly of section .text:

0042a0b0 <.text+0x290b0>:
  42a0b0:	83 ec 0c             	sub    $0xc,%esp
  42a0b3:	55                   	push   %ebp
  42a0b4:	57                   	push   %edi
  42a0b5:	8b f9                	mov    %ecx,%edi
  42a0b7:	8b 47 4c             	mov    0x4c(%edi),%eax
  42a0ba:	8d 6f 4c             	lea    0x4c(%edi),%ebp
  42a0bd:	8b cd                	mov    %ebp,%ecx
  42a0bf:	ff 50 18             	call   *0x18(%eax)
  42a0c2:	a8 02                	test   $0x2,%al
  42a0c4:	0f 84 b7 02 00 00    	je     0x42a381
  42a0ca:	56                   	push   %esi
  42a0cb:	8b b7 e0 00 00 00    	mov    0xe0(%edi),%esi
  42a0d1:	85 f6                	test   %esi,%esi
  42a0d3:	0f 84 a7 02 00 00    	je     0x42a380
  42a0d9:	8b 16                	mov    (%esi),%edx
  42a0db:	6a 00                	push   $0x0
  42a0dd:	8b ce                	mov    %esi,%ecx
  42a0df:	ff 52 18             	call   *0x18(%edx)
  42a0e2:	8b 06                	mov    (%esi),%eax
  42a0e4:	6a 00                	push   $0x0
  42a0e6:	8b ce                	mov    %esi,%ecx
  42a0e8:	ff 50 1c             	call   *0x1c(%eax)
  42a0eb:	8b 16                	mov    (%esi),%edx
  42a0ed:	6a 00                	push   $0x0
  42a0ef:	8b ce                	mov    %esi,%ecx
  42a0f1:	ff 52 24             	call   *0x24(%edx)
  42a0f4:	8b 06                	mov    (%esi),%eax
  42a0f6:	6a 00                	push   $0x0
  42a0f8:	8b ce                	mov    %esi,%ecx
  42a0fa:	ff 50 2c             	call   *0x2c(%eax)
  42a0fd:	8b 55 00             	mov    0x0(%ebp),%edx
  42a100:	8b cd                	mov    %ebp,%ecx
  42a102:	ff 52 08             	call   *0x8(%edx)
  42a105:	8b 10                	mov    (%eax),%edx
  42a107:	8b c8                	mov    %eax,%ecx
  42a109:	ff 52 20             	call   *0x20(%edx)
  42a10c:	84 c0                	test   %al,%al
  42a10e:	74 0f                	je     0x42a11f
  42a110:	c7 87 50 07 00 00 cd 	movl   $0x3f4ccccd,0x750(%edi)
  42a117:	cc 4c 3f
  42a11a:	e9 5a 01 00 00       	jmp    0x42a279
  42a11f:	8a 87 a7 00 00 00    	mov    0xa7(%edi),%al
  42a125:	84 c0                	test   %al,%al
  42a127:	75 25                	jne    0x42a14e
  42a129:	8a 87 a8 00 00 00    	mov    0xa8(%edi),%al
  42a12f:	84 c0                	test   %al,%al
  42a131:	74 1b                	je     0x42a14e
  42a133:	c7 87 50 07 00 00 00 	movl   $0x3f800000,0x750(%edi)
  42a13a:	00 80 3f
  42a13d:	8b 06                	mov    (%esi),%eax
  42a13f:	68 00 00 80 3f       	push   $0x3f800000
  42a144:	8b ce                	mov    %esi,%ecx
  42a146:	ff 50 24             	call   *0x24(%eax)
  42a149:	e9 2b 01 00 00       	jmp    0x42a279
  42a14e:	8b 8f d4 00 00 00    	mov    0xd4(%edi),%ecx
  42a154:	85 c9                	test   %ecx,%ecx
  42a156:	53                   	push   %ebx
  42a157:	74 0d                	je     0x42a166
  42a159:	8b 11                	mov    (%ecx),%edx
  42a15b:	ff 52 14             	call   *0x14(%edx)
  42a15e:	84 c0                	test   %al,%al
  42a160:	74 04                	je     0x42a166
  42a162:	b3 01                	mov    $0x1,%bl
  42a164:	eb 02                	jmp    0x42a168
  42a166:	32 db                	xor    %bl,%bl
  42a168:	8b 45 00             	mov    0x0(%ebp),%eax
  42a16b:	8b cd                	mov    %ebp,%ecx
  42a16d:	ff 50 08             	call   *0x8(%eax)
  42a170:	8b 10                	mov    (%eax),%edx
  42a172:	8b c8                	mov    %eax,%ecx
  42a174:	ff 92 90 00 00 00    	call   *0x90(%edx)
  42a17a:	d9 5c 24 10          	fstps  0x10(%esp)
  42a17e:	8b 87 84 00 00 00    	mov    0x84(%edi),%eax
  42a184:	d9 44 24 10          	flds   0x10(%esp)
  42a188:	8b 4c 24 20          	mov    0x20(%esp),%ecx
  42a18c:	89 44 24 14          	mov    %eax,0x14(%esp)
  42a190:	d8 64 24 14          	fsubs  0x14(%esp)
  42a194:	6a 00                	push   $0x0
  42a196:	6a 00                	push   $0x0
  42a198:	51                   	push   %ecx
  42a199:	8b 8f 44 07 00 00    	mov    0x744(%edi),%ecx
  42a19f:	d9 5c 24 24          	fstps  0x24(%esp)
  42a1a3:	8b 54 24 24          	mov    0x24(%esp),%edx
  42a1a7:	52                   	push   %edx
  42a1a8:	e8 73 9c 22 00       	call   0x653e20
  42a1ad:	d9 44 24 14          	flds   0x14(%esp)
  42a1b1:	d8 1d 34 06 89 00    	fcomps 0x890634
  42a1b7:	df e0                	fnstsw %ax
  42a1b9:	f6 c4 05             	test   $0x5,%ah
  42a1bc:	7a 0f                	jp     0x42a1cd
  42a1be:	c7 87 50 07 00 00 00 	movl   $0xbf800000,0x750(%edi)
  42a1c5:	00 80 bf
  42a1c8:	e9 ab 00 00 00       	jmp    0x42a278
  42a1cd:	84 db                	test   %bl,%bl
  42a1cf:	d9 44 24 10          	flds   0x10(%esp)
  42a1d3:	74 28                	je     0x42a1fd
  42a1d5:	d8 1d 6c 09 89 00    	fcomps 0x89096c
  42a1db:	df e0                	fnstsw %ax
  42a1dd:	f6 c4 41             	test   $0x41,%ah
  42a1e0:	75 0f                	jne    0x42a1f1
  42a1e2:	c7 87 50 07 00 00 00 	movl   $0xbf800000,0x750(%edi)
  42a1e9:	00 80 bf
  42a1ec:	e9 87 00 00 00       	jmp    0x42a278
  42a1f1:	c7 87 50 07 00 00 00 	movl   $0x3f800000,0x750(%edi)
  42a1f8:	00 80 3f
  42a1fb:	eb 7b                	jmp    0x42a278
  42a1fd:	d8 1d a8 0d 89 00    	fcomps 0x890da8
  42a203:	df e0                	fnstsw %ax
  42a205:	f6 c4 05             	test   $0x5,%ah
  42a208:	7a 0c                	jp     0x42a216
  42a20a:	c7 87 50 07 00 00 00 	movl   $0xbf800000,0x750(%edi)
  42a211:	00 80 bf
  42a214:	eb 62                	jmp    0x42a278
  42a216:	8b 9f 44 07 00 00    	mov    0x744(%edi),%ebx
  42a21c:	68 00 00 a0 40       	push   $0x40a00000
  42a221:	68 00 00 a0 c0       	push   $0xc0a00000
  42a226:	8b cb                	mov    %ebx,%ecx
  42a228:	e8 13 9d fd ff       	call   0x403f40
  42a22d:	51                   	push   %ecx
  42a22e:	d9 1c 24             	fstps  (%esp)
  42a231:	e8 2a 7e fd ff       	call   0x402060
  42a236:	8b 43 5c             	mov    0x5c(%ebx),%eax
  42a239:	d8 0d 10 08 89 00    	fmuls  0x890810
  42a23f:	68 00 00 20 41       	push   $0x41200000
  42a244:	68 00 00 20 c1       	push   $0xc1200000
  42a249:	d9 5c 24 34          	fstps  0x34(%esp)
  42a24d:	50                   	push   %eax
  42a24e:	e8 0d 7e fd ff       	call   0x402060
  42a253:	d8 0d 98 16 89 00    	fmuls  0x891698
  42a259:	83 c4 18             	add    $0x18,%esp
  42a25c:	d8 44 24 20          	fadds  0x20(%esp)
  42a260:	d9 44 24 18          	flds   0x18(%esp)
  42a264:	d8 0d 08 08 89 00    	fmuls  0x890808
  42a26a:	de c1                	faddp  %st,%st(1)
  42a26c:	d8 87 50 07 00 00    	fadds  0x750(%edi)
  42a272:	d9 9f 50 07 00 00    	fstps  0x750(%edi)
  42a278:	5b                   	pop    %ebx
  42a279:	8b 97 50 07 00 00    	mov    0x750(%edi),%edx
  42a27f:	8b                   	.byte 0x8b
