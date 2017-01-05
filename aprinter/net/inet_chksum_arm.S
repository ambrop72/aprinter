	.cpu cortex-m3
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 1
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.file	"inet_chksum_arm.c"
	.text
	.align	1
	.global	IpChksumInverted
	.syntax unified
	.thumb
	.thumb_func
	.fpu softvfp
	.type	IpChksumInverted, %function
IpChksumInverted:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	push	{r4, r5, r6, r7, lr}
	tst	r0, #3
	beq	.L14
	tst	r0, #1
	beq	.L15
	cmp	r1, #0
	beq	.L16
	ldrb	r3, [r0], #1	@ zero_extendqisi2
	lsls	r3, r3, #8
	subs	r1, r1, #1
	movs	r6, #1
	b	.L3
.L15:
	movs	r3, #0
	mov	r6, r3
.L3:
	tst	r0, #2
	beq	.L2
	cmp	r1, #1
	bls	.L2
	ldrh	r2, [r0], #2
	add	r3, r3, r2
	subs	r1, r1, #2
	b	.L2
.L14:
	movs	r3, #0
	mov	r6, r3
.L2:
	cmp	r1, #63
	bls	.L17
	mov	r4, r1
.L5:
	.syntax unified
@ 180 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldmia   r0!, {r2, r5, r7, lr}   
        adds    r3,r3,r2; adcs r3,r3,r5 
        adcs    r3,r3,r7; adcs r3,r3,lr 
        ldmia   r0!, {r2, r5, r7, lr}   
        adcs    r3,r3,r2; adcs r3,r3,r5 
        adcs    r3,r3,r7; adcs r3,r3,lr 
        ldmia   r0!, {r2, r5, r7, lr}   
        adcs    r3,r3,r2; adcs r3,r3,r5 
        adcs    r3,r3,r7; adcs r3,r3,lr 
        ldmia   r0!, {r2, r5, r7, lr}   
        adcs    r3,r3,r2; adcs r3,r3,r5 
        adcs    r3,r3,r7; adcs r3,r3,lr 
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r4, r4, #64
	cmp	r4, #63
	bhi	.L5
	and	r1, r1, #63
.L4:
	cmp	r1, #31
	bls	.L6
	.syntax unified
@ 184 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldmia   r0!, {r2, r4, r5, r7}   
        adds    r3,r3,r2; adcs r3,r3,r4 
        adcs    r3,r3,r5; adcs r3,r3,r7 
        ldmia   r0!, {r2, r4, r5, r7}   
        adcs    r3,r3,r2; adcs r3,r3,r4 
        adcs    r3,r3,r5; adcs r3,r3,r7 
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #32
.L6:
	cmp	r1, #15
	bls	.L7
	.syntax unified
@ 188 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldmia   r0!, {r2, r4, r5, r7}   
        adds    r3,r3,r2; adcs r3,r3,r4 
        adcs    r3,r3,r5; adcs r3,r3,r7 
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #16
.L7:
	cmp	r1, #7
	bls	.L8
	.syntax unified
@ 192 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldmia   r0!, {r2, r4}           
        adds    r3,r3,r2; adcs r3,r3,r4 
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #8
.L8:
	cmp	r1, #3
	bls	.L9
	.syntax unified
@ 196 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldr     r2,[r0],#4              
        adds    r3,r3,r2                
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #4
.L9:
	cbz	r1, .L10
	.syntax unified
@ 200 "aprinter/net/inet_chksum_arm.c" 1
	      
        mov     r2, #0x00ff             
        orr     r2, r2, #0xff00         
        and     r2, r3, r2              
        add     r3, r2, r3, lsr #16

@ 0 "" 2
	.thumb
	.syntax unified
	cmp	r1, #1
	bls	.L11
	ldrh	r4, [r0], #2
	add	r3, r3, r4
	subs	r1, r1, #2
.L11:
	cmp	r1, #1
	beq	.L20
.L10:
	cbnz	r6, .L12
	.syntax unified
@ 211 "aprinter/net/inet_chksum_arm.c" 1
	      
        mov     r2, #0x00ff             
        orr     r2, r2, #0xff00         
        and     r2, r3, r2              
        add     r3, r2, r3, lsr #16

@ 0 "" 2
	.thumb
	.syntax unified
	lsls	r3, r3, #8
.L12:
	.syntax unified
@ 214 "aprinter/net/inet_chksum_arm.c" 1
	      
        mov     r2, #0x00ff             
        orr     r2, r2, #0xff00         
        and     r2, r3, r2              
        add     r3, r2, r3, lsr #16

@ 0 "" 2
	.thumb
	.syntax unified
	cmp	r3, #65536
	itt	cs
	subcs	r3, r3, #65280
	subcs	r3, r3, #255
	uxth	r0, r3
	pop	{r4, r5, r6, r7, pc}
.L16:
	movs	r6, #0
	mov	r2, r6
	mov	r3, r6
	b	.L4
.L17:
	movs	r2, #0
	b	.L4
.L20:
	ldrb	r1, [r0]	@ zero_extendqisi2
	lsls	r1, r1, #8
	add	r3, r1, r3, lsl #8
	eor	r6, r6, #1
	b	.L10
	.size	IpChksumInverted, .-IpChksumInverted
	.ident	"GCC: (GNU) 6.2.0"
