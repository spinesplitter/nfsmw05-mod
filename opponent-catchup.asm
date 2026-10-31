
t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\speed.exe:     file format pei-i386


Disassembly of section .text:

00409300 <.text+0x8300>:
  409300:	33 ff                	xor    %edi,%edi
  409302:	89 7e 44             	mov    %edi,0x44(%esi)
  409305:	5f                   	pop    %edi
  409306:	89 4e 1c             	mov    %ecx,0x1c(%esi)
  409309:	5e                   	pop    %esi
  40930a:	5d                   	pop    %ebp
  40930b:	5b                   	pop    %ebx
  40930c:	8b 4c 24 04          	mov    0x4(%esp),%ecx
  409310:	64 89 0d 00 00 00 00 	mov    %ecx,%fs:0x0
  409317:	83 c4 10             	add    $0x10,%esp
  40931a:	c2 04 00             	ret    $0x4
  40931d:	8b 54 24 24          	mov    0x24(%esp),%edx
  409321:	8b 4c 24 14          	mov    0x14(%esp),%ecx
  409325:	5f                   	pop    %edi
  409326:	89 56 1c             	mov    %edx,0x1c(%esi)
  409329:	5e                   	pop    %esi
  40932a:	5d                   	pop    %ebp
  40932b:	5b                   	pop    %ebx
  40932c:	64 89 0d 00 00 00 00 	mov    %ecx,%fs:0x0
  409333:	83 c4 10             	add    $0x10,%esp
  409336:	c2 04 00             	ret    $0x4
  409339:	cc                   	int3
  40933a:	cc                   	int3
  40933b:	cc                   	int3
  40933c:	cc                   	int3
  40933d:	cc                   	int3
  40933e:	cc                   	int3
  40933f:	cc                   	int3
  409340:	51                   	push   %ecx
  409341:	d9 81 60 07 00 00    	flds   0x760(%ecx)
  409347:	d8 81 5c 07 00 00    	fadds  0x75c(%ecx)
  40934d:	d9 14 24             	fsts   (%esp)
  409350:	d8 1d 68 09 89 00    	fcomps 0x890968
  409356:	df e0                	fnstsw %ax
  409358:	f6 c4 41             	test   $0x41,%ah
  40935b:	75 1d                	jne    0x40937a
  40935d:	d9 04 24             	flds   (%esp)
  409360:	d9 05 6c 09 89 00    	flds   0x89096c
  409366:	d8 1c 24             	fcomps (%esp)
  409369:	df e0                	fnstsw %ax
  40936b:	f6 c4 05             	test   $0x5,%ah
  40936e:	7a 08                	jp     0x409378
  409370:	dd d8                	fstp   %st(0)
  409372:	d9 05 6c 09 89 00    	flds   0x89096c
  409378:	59                   	pop    %ecx
  409379:	c3                   	ret
  40937a:	d9 05 68 09 89 00    	flds   0x890968
  409380:	59                   	pop    %ecx
  409381:	c3                   	ret
  409382:	cc                   	int3
  409383:	cc                   	int3
  409384:	cc                   	int3
  409385:	cc                   	int3
  409386:	cc                   	int3
  409387:	cc                   	int3
  409388:	cc                   	int3
  409389:	cc                   	int3
  40938a:	cc                   	int3
  40938b:	cc                   	int3
  40938c:	cc                   	int3
  40938d:	cc                   	int3
  40938e:	cc                   	int3
  40938f:	cc                   	int3
  409390:	51                   	push   %ecx
  409391:	d9 41 40             	flds   0x40(%ecx)
  409394:	d8 41 3c             	fadds  0x3c(%ecx)
  409397:	d8 25 6c 09 89 00    	fsubs  0x89096c
  40939d:	d9 14 24             	fsts   (%esp)
  4093a0:	d8 1d 6c 09 89 00    	fcomps 0x89096c
  4093a6:	df e0                	fnstsw %ax
  4093a8:	f6 c4 05             	test   $0x5,%ah
  4093ab:	7a 30                	jp     0x4093dd
  4093ad:	d9 04 24             	flds   (%esp)
  4093b0:	d9 05 68 09 89 00    	flds   0x890968
  4093b6:	d8 1c 24             	fcomps (%esp)
  4093b9:	df e0                	fnstsw %ax
  4093bb:	f6 c4 41             	test   $0x41,%ah
  4093be:	75 23                	jne    0x4093e3
  4093c0:	8b 41 3c             	mov    0x3c(%ecx),%eax
  4093c3:	dd d8                	fstp   %st(0)
  4093c5:	50                   	push   %eax
  4093c6:	b9 fc b4 8e 00       	mov    $0x8eb4fc,%ecx
  4093cb:	c7 44 24 04 00 00 00 	movl   $0x0,0x4(%esp)
  4093d2:	00
  4093d3:	e8 d8 9f 24 00       	call   0x6533b0
  4093d8:	d8 0c 24             	fmuls  (%esp)
  4093db:	59                   	pop    %ecx
  4093dc:	c3                   	ret
  4093dd:	d9 05 6c 09 89 00    	flds   0x89096c
  4093e3:	8b 41 3c             	mov    0x3c(%ecx),%eax
  4093e6:	d9 1c 24             	fstps  (%esp)
  4093e9:	50                   	push   %eax
  4093ea:	b9 fc b4 8e 00       	mov    $0x8eb4fc,%ecx
  4093ef:	e8 bc 9f 24 00       	call   0x6533b0
  4093f4:	d8 0c 24             	fmuls  (%esp)
  4093f7:	59                   	pop    %ecx
  4093f8:	c3                   	ret
  4093f9:	cc                   	int3
  4093fa:	cc                   	int3
  4093fb:	cc                   	int3
  4093fc:	cc                   	int3
  4093fd:	cc                   	int3
  4093fe:	cc                   	int3
  4093ff:	cc                   	int3
  409400:	d9 41 1c             	flds   0x1c(%ecx)
  409403:	c3                   	ret
  409404:	cc                   	int3
  409405:	cc                   	int3
  409406:	cc                   	int3
  409407:	cc                   	int3
  409408:	cc                   	int3
  409409:	cc                   	int3
  40940a:	cc                   	int3
  40940b:	cc                   	int3
  40940c:	cc                   	int3
  40940d:	cc                   	int3
  40940e:	cc                   	int3
  40940f:	cc                   	int3
  409410:	a1 00 e0 91 00       	mov    0x91e000,%eax
  409415:	85 c0                	test   %eax,%eax
  409417:	57                   	push   %edi
  409418:	8b f9                	mov    %ecx,%edi
  40941a:	74 5f                	je     0x40947b
  40941c:	8b 87 f4 f8 ff ff    	mov    -0x70c(%edi),%eax
  409422:	8d 8f f4 f8 ff ff    	lea    -0x70c(%edi),%ecx
  409428:	56                   	push   %esi
  409429:	ff 90 a0 00 00 00    	call   *0xa0(%eax)
  40942f:	8b f0                	mov    %eax,%esi
  409431:	85 f6                	test   %esi,%esi
  409433:	74 45                	je     0x40947a
  409435:	8b 0d 00 e0 91 00    	mov    0x91e000,%ecx
  40943b:	8b 89 68 19 00 00    	mov    0x1968(%ecx),%ecx
  409441:	53                   	push   %ebx
  409442:	32 db                	xor    %bl,%bl
  409444:	85 c9                	test   %ecx,%ecx
  409446:	74 0c                	je     0x409454
  409448:	e8 d3 15 1f 00       	call   0x5faa20
  40944d:	83 f8 08             	cmp    $0x8,%eax
  409450:	75 02                	jne    0x409454
  409452:	b3 01                	mov    $0x1,%bl
  409454:	8b 15 00 e0 91 00    	mov    0x91e000,%edx
  40945a:	83 ba 64 19 00 00 02 	cmpl   $0x2,0x1964(%edx)
  409461:	74 04                	je     0x409467
  409463:	84 db                	test   %bl,%bl
  409465:	74 12                	je     0x409479
  409467:	8b 44 24 10          	mov    0x10(%esp),%eax
  40946b:	01 47 20             	add    %eax,0x20(%edi)
  40946e:	8b 16                	mov    (%esi),%edx
  409470:	50                   	push   %eax
  409471:	8b ce                	mov    %esi,%ecx
  409473:	ff 92 f4 00 00 00    	call   *0xf4(%edx)
  409479:	5b                   	pop    %ebx
  40947a:	5e                   	pop    %esi
  40947b:	5f                   	pop    %edi
  40947c:	c2 04 00             	ret    $0x4
  40947f:	cc                   	int3
  409480:	a1 00 e0 91 00       	mov    0x91e000,%eax
  409485:	85 c0                	test   %eax,%eax
  409487:	56                   	push   %esi
  409488:	8b f1                	mov    %ecx,%esi
  40948a:	74 2a                	je     0x4094b6
  40948c:	8b 88 60 19 00 00    	mov    0x1960(%eax),%ecx
  409492:	85 c9                	test   %ecx,%ecx
  409494:	74 14                	je     0x4094aa
  409496:	8b 88 68 19 00 00    	mov    0x1968(%eax),%ecx
  40949c:	85 c9                	test   %ecx,%ecx
  40949e:	74 16                	je     0x4094b6
  4094a0:	e8 7b 15 1f 00       	call   0x5faa20
  4094a5:	83 f8 08             	cmp    $0x8,%eax
  4094a8:	75 0c                	jne    0x4094b6
  4094aa:	8b 4e 24             	mov    0x24(%esi),%ecx
  4094ad:	8b                   	.byte 0x8b
  4094ae:	44                   	inc    %esp
  4094af:	24                   	.byte 0x24
