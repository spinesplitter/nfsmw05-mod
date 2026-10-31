
t:\Games\ModWanted\1cookie Need For Speed Most Wanted Black Edition -\speed.exe:     file format pei-i386


Disassembly of section .text:

0041ec10 <.text+0x1dc10>:
  41ec10:	00 2b                	add    %ch,(%ebx)
  41ec12:	c5 83 f8 14 5e 1b    	lds    0x1b5e14f8(%ebx),%eax
  41ec18:	c0 5d f7 d8          	rcrb   $0xd8,-0x9(%ebp)
  41ec1c:	5b                   	pop    %ebx
  41ec1d:	c3                   	ret
  41ec1e:	cc                   	int3
  41ec1f:	cc                   	int3
  41ec20:	a1 c8 85 98 00       	mov    0x9885c8,%eax
  41ec25:	85 c0                	test   %eax,%eax
  41ec27:	75 13                	jne    0x41ec3c
  41ec29:	8b 0d 7c c6 92 00    	mov    0x92c67c,%ecx
  41ec2f:	85 c9                	test   %ecx,%ecx
  41ec31:	74 10                	je     0x41ec43
  41ec33:	8b 01                	mov    (%ecx),%eax
  41ec35:	ff 50 14             	call   *0x14(%eax)
  41ec38:	84 c0                	test   %al,%al
  41ec3a:	74 07                	je     0x41ec43
  41ec3c:	d9 05 68 09 89 00    	flds   0x890968
  41ec42:	c3                   	ret
  41ec43:	a1 64 60 92 00       	mov    0x926064,%eax
  41ec48:	85 c0                	test   %eax,%eax
  41ec4a:	74 29                	je     0x41ec75
  41ec4c:	a1 94 60 92 00       	mov    0x926094,%eax
  41ec51:	85 c0                	test   %eax,%eax
  41ec53:	75 20                	jne    0x41ec75
  41ec55:	db 05 90 60 92 00    	fildl  0x926090
  41ec5b:	68 00 00 80 3f       	push   $0x3f800000
  41ec60:	6a 00                	push   $0x0
  41ec62:	51                   	push   %ecx
  41ec63:	d8 0d c4 0d 89 00    	fmuls  0x890dc4
  41ec69:	d9 1c 24             	fstps  (%esp)
  41ec6c:	e8 ff 30 fe ff       	call   0x401d70
  41ec71:	83 c4 0c             	add    $0xc,%esp
  41ec74:	c3                   	ret
  41ec75:	8b 0d 00 e0 91 00    	mov    0x91e000,%ecx
  41ec7b:	d9 05 68 09 89 00    	flds   0x890968
  41ec81:	85 c9                	test   %ecx,%ecx
  41ec83:	74 3a                	je     0x41ecbf
  41ec85:	8b 81 60 19 00 00    	mov    0x1960(%ecx),%eax
  41ec8b:	83 f8 01             	cmp    $0x1,%eax
  41ec8e:	75 23                	jne    0x41ecb3
  41ec90:	dd d8                	fstp   %st(0)
  41ec92:	68 00 00 80 3f       	push   $0x3f800000
  41ec97:	db 81 4c 45 00 00    	fildl  0x454c(%ecx)
  41ec9d:	6a 00                	push   $0x0
  41ec9f:	51                   	push   %ecx
  41eca0:	d8 0d c4 0d 89 00    	fmuls  0x890dc4
  41eca6:	d9 1c 24             	fstps  (%esp)
  41eca9:	e8 c2 30 fe ff       	call   0x401d70
  41ecae:	83 c4 0c             	add    $0xc,%esp
  41ecb1:	eb 0c                	jmp    0x41ecbf
  41ecb3:	85 c0                	test   %eax,%eax
  41ecb5:	75 08                	jne    0x41ecbf
  41ecb7:	dd d8                	fstp   %st(0)
  41ecb9:	d9 05 6c 09 89 00    	flds   0x89096c
  41ecbf:	a1 34 fd 92 00       	mov    0x92fd34,%eax
  41ecc4:	85 c0                	test   %eax,%eax
  41ecc6:	74 06                	je     0x41ecce
  41ecc8:	d8 0d 50 10 89 00    	fmuls  0x891050
  41ecce:	c3                   	ret
  41eccf:	cc                   	int3
