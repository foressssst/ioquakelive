/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "qasm.h"

#if	id386

	.text

#if 0 

#define ch		4+16
#define sc		8+16
#define count	12+16

.globl C(S_PaintChannelFrom8)
C(S_PaintChannelFrom8):
	pushl	%esi
	pushl	%edi
	pushl	%ebx
	pushl	%ebp

	movl	ch(%esp),%ebx
	movl	sc(%esp),%esi

	movl	ch_leftvol(%ebx),%eax
	movl	ch_rightvol(%ebx),%edx
	cmpl	$255,%eax
	jna		LLeftSet
	movl	$255,%eax
LLeftSet:
	cmpl	$255,%edx
	jna		LRightSet
	movl	$255,%edx
LRightSet:

	andl	$0xF8,%eax
	addl	$20,%esi
	movl	(%esi),%esi
	andl	$0xF8,%edx
	movl	ch_pos(%ebx),%edi
	movl	count(%esp),%ecx
	addl	%edi,%esi
	shll	$7,%eax
	addl	%ecx,%edi
	shll	$7,%edx
	movl	%edi,ch_pos(%ebx)
	addl	$(C(snd_scaletable)),%eax
	addl	$(C(snd_scaletable)),%edx
	subl	%ebx,%ebx
	movb	-1(%esi,%ecx,1),%bl

	testl	$1,%ecx
	jz		LMix8Loop

	movl	(%eax,%ebx,4),%edi
	movl	(%edx,%ebx,4),%ebp
	addl	C(paintbuffer)+psp_left-psp_size(,%ecx,psp_size),%edi
	addl	C(paintbuffer)+psp_right-psp_size(,%ecx,psp_size),%ebp
	movl	%edi,C(paintbuffer)+psp_left-psp_size(,%ecx,psp_size)
	movl	%ebp,C(paintbuffer)+psp_right-psp_size(,%ecx,psp_size)
	movb	-2(%esi,%ecx,1),%bl

	decl	%ecx
	jz		LDone

LMix8Loop:

	movl	(%eax,%ebx,4),%edi
	movl	(%edx,%ebx,4),%ebp
	addl	C(paintbuffer)+psp_left-psp_size(,%ecx,psp_size),%edi
	addl	C(paintbuffer)+psp_right-psp_size(,%ecx,psp_size),%ebp
	movb	-2(%esi,%ecx,1),%bl
	movl	%edi,C(paintbuffer)+psp_left-psp_size(,%ecx,psp_size)
	movl	%ebp,C(paintbuffer)+psp_right-psp_size(,%ecx,psp_size)

	movl	(%eax,%ebx,4),%edi
	movl	(%edx,%ebx,4),%ebp
	movb	-3(%esi,%ecx,1),%bl
	addl	C(paintbuffer)+psp_left-psp_size*2(,%ecx,psp_size),%edi
	addl	C(paintbuffer)+psp_right-psp_size*2(,%ecx,psp_size),%ebp
	movl	%edi,C(paintbuffer)+psp_left-psp_size*2(,%ecx,psp_size)
	movl	%ebp,C(paintbuffer)+psp_right-psp_size*2(,%ecx,psp_size)

	subl	$2,%ecx
	jnz		LMix8Loop

LDone:
	popl	%ebp
	popl	%ebx
	popl	%edi
	popl	%esi

	ret

#endif

.globl C(S_WriteLinearBlastStereo16)
C(S_WriteLinearBlastStereo16):
	pushl	%edi
	pushl	%ebx

	movl	C(snd_linear_count),%ecx
	movl	C(snd_p),%ebx
	movl	C(snd_out),%edi

LWLBLoopTop:

	movl	-8(%ebx,%ecx,4),%eax
	sarl	$8,%eax
	cmpl	$0x7FFF,%eax
	jg		LClampHigh
	cmpl	$0xFFFF8000,%eax
	jnl		LClampDone
	movl	$0xFFFF8000,%eax
	jmp		LClampDone
LClampHigh:
	movl	$0x7FFF,%eax
LClampDone:

	movl	-4(%ebx,%ecx,4),%edx
	sarl	$8,%edx
	cmpl	$0x7FFF,%edx
	jg		LClampHigh2
	cmpl	$0xFFFF8000,%edx
	jnl		LClampDone2
	movl	$0xFFFF8000,%edx
	jmp		LClampDone2
LClampHigh2:
	movl	$0x7FFF,%edx
LClampDone2:
	shll	$16,%edx
	andl	$0xFFFF,%eax
	orl		%eax,%edx
	movl	%edx,-4(%edi,%ecx,2)

	subl	$2,%ecx
	jnz		LWLBLoopTop

	popl	%ebx
	popl	%edi

	ret

#endif

