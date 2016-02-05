/* Compiled manually from inet_chksum_arm.c.
 * See that file for copyright info.
 */
	.syntax unified
	.cpu cortex-m3
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 1
	.eabi_attribute 30, 2
	.eabi_attribute 34, 1
	.eabi_attribute 18, 4
	.thumb
	.syntax unified
	.file	"inet_chksum_arm.c"
	.text
	.align	2
	.global	lwip_standard_chksum
	.thumb
	.thumb_func
	.type	lwip_standard_chksum, %function
lwip_standard_chksum:
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 0, uses_anonymous_args = 0
	ands	r3, r0, #3
	push	{r4, r5, r6, r7, lr}
	beq	.L16
	ands	r3, r0, #1
	bne	.L33
	mov	r6, r3
.L3:
	lsls	r2, r0, #30
	bpl	.L2
	cmp	r1, #1
	ble	.L5
	ldrh	r2, [r0], #2
	subs	r1, r1, #2
	add	r3, r3, r2
	b	.L2
.L16:
	mov	r6, r3
.L2:
	cmp	r1, #63
	ble	.L18
	mov	r4, r1
.L7:
	.syntax unified
@ 170 "aprinter/net/inet_chksum_arm.c" 1
	      
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
	bgt	.L7
	and	r1, r1, #63
.L6:
	cmp	r1, #31
	ble	.L8
	.syntax unified
@ 174 "aprinter/net/inet_chksum_arm.c" 1
	      
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
.L8:
	cmp	r1, #15
	ble	.L9
	.syntax unified
@ 178 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldmia   r0!, {r2, r4, r5, r7}   
        adds    r3,r3,r2; adcs r3,r3,r4 
        adcs    r3,r3,r5; adcs r3,r3,r7 
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #16
.L9:
	cmp	r1, #7
	ble	.L10
	.syntax unified
@ 182 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldmia   r0!, {r2, r4}           
        adds    r3,r3,r2; adcs r3,r3,r4 
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #8
.L10:
	cmp	r1, #3
	ble	.L11
	.syntax unified
@ 186 "aprinter/net/inet_chksum_arm.c" 1
	      
        ldr     r2,[r0],#4              
        adds    r3,r3,r2                
        adcs    r3,r3,#0

@ 0 "" 2
	.thumb
	.syntax unified
	subs	r1, r1, #4
.L11:
	cbz	r1, .L12
	.syntax unified
@ 190 "aprinter/net/inet_chksum_arm.c" 1
	      
        mov     r2, #0x00ff             
        orr     r2, r2, #0xff00         
        and     r2, r3, r2              
        add     r3, r2, r3, lsr #16

@ 0 "" 2
	.thumb
	.syntax unified
	cmp	r1, #1
	ble	.L13
	ldrh	r4, [r0], #2
	subs	r1, r1, #2
	add	r3, r3, r4
.L13:
	cmp	r1, #1
	beq	.L34
.L12:
	cbz	r6, .L14
	.syntax unified
@ 201 "aprinter/net/inet_chksum_arm.c" 1
	      
        mov     r2, #0x00ff             
        orr     r2, r2, #0xff00         
        and     r2, r3, r2              
        add     r3, r2, r3, lsr #16

@ 0 "" 2
	.thumb
	.syntax unified
	lsls	r3, r3, #8
.L14:
	.syntax unified
@ 204 "aprinter/net/inet_chksum_arm.c" 1
	      
        mov     r2, #0x00ff             
        orr     r2, r2, #0xff00         
        and     r2, r3, r2              
        add     r3, r2, r3, lsr #16

@ 0 "" 2
	.thumb
	.syntax unified
	cmp	r3, #65536
	bcc	.L15
	sub	r3, r3, #65280
	subs	r3, r3, #255
.L15:
	uxth	r0, r3
	pop	{r4, r5, r6, r7, pc}
.L33:
	cmp	r1, #0
	ble	.L4
	ldrb	r3, [r0], #1	@ zero_extendqisi2
	subs	r1, r1, #1
	lsls	r3, r3, #8
	movs	r6, #1
	b	.L3
.L34:
	ldrb	r1, [r0]	@ zero_extendqisi2
	eor	r6, r6, #1
	lsls	r1, r1, #8
	add	r3, r1, r3, lsl #8
	b	.L12
.L4:
	ands	r6, r0, #2
	beq	.L19
	movs	r3, #0
	mov	r6, r3
.L5:
	movs	r2, #0
	b	.L8
.L19:
	mov	r2, r6
	mov	r3, r6
	b	.L6
.L18:
	movs	r2, #0
	b	.L6
	.size	lwip_standard_chksum, .-lwip_standard_chksum
	.ident	"GCC: (GNU) 5.3.0"
