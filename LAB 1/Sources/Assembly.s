		AREA	text, CODE, READONLY
		EXPORT IIR_asm

;y[n] = b0 * x[n] + b1 * x[n-1] + b2 * x[n-2] + a1 * y[n-1] + a2 * y[n-2]
;R0 = pIn
;R1 = pOut
;R2 = pCoeffs
;R3 = length
;R4 = counter
;S5 is used for coeffs
IIR_asm
		CMP R3, #0
		BEQ endProg

		MOV R4, #0	;init counter to 0
		
		;init states
		VLDR.F32 S0, =0.0	;x[n-1]
		VLDR.F32 S1, =0.0	;x[n-2]
		VLDR.F32 S2, =0.0	;y[n-1]
		VLDR.F32 S3, =0.0	;y[n-2]

loop
		;calc
		VLDR S4, [R0]	;x[n]
		VLDR S5, [R2]	;b0
		VMUL.F32 S4, S4, S5	;S4 = x[n]*b0
		
		VLDR S5, [R2, #4]	;b1
		VMLA.F32 S4, S5, S0	;S4 = x[n]*b0 + x[n-1]*b1
		
		VLDR S5, [R2, #8]	;b2
		VMLA.F32 S4, S5, S1	;S4 = x[n]*b0 + x[n-1]*b1 + x[n-2]*b2
		
		VLDR S5, [R2, #12]	;a1
		VMLA.F32 S4, S5, S2	;S4 = x[n]*b0 + x[n-1]*b1 + x[n-2]*b2 + y[n-1]*a1
		
		VLDR S5, [R2, #16]	;a2
		VMLA.F32 S4, S5, S3	;S4 = x[n]*b0 + x[n-1]*b1 + x[n-2]*b2 + y[n-1]*a1 + y[n-2]*a2 ==> y[n]
		
		VSTR S4, [R1]	;save output, y[n] = S4
		
		
		;update states
		VMOV.F32 S1, S0	;S1 = S0 ==> x[n-2] = x[n-1]
		VMOV.F32 S3, S2	;S3 = S2 ==> y[n-2] = y[n-1]
		VMOV.F32 S2, S4	;S2 = S4 ==> y[n-1] = y[n]
		VLDR S0, [R0]	;S0 = x[n-1]
		
		
		;increment and check
		ADD R0, R0, #4	;change pointer to next x[n] corresponding to the next y[n]
		ADD R1, R1, #4	;change pointer to next y[n]
		ADD R4, R4, #1	;increment counter
		
		CMP R4, R3	;while(counter != length)
		BNE loop	;loop
		
endProg
		BX LR;
		END
