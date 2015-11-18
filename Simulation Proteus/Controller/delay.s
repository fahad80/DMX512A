;Delay Example
;Jason Losh

;------------------------------------------------------------------------------
; Objectives and notes             
;------------------------------------------------------------------------------


;------------------------------------------------------------------------------
; Device includes and assembler directives             
;------------------------------------------------------------------------------

          .include "p33FJ32MC202.inc"
          .global _wait_us
          .global _wait_ms
          .global _wait_52083ns
		  .global _change_osc

;------------------------------------------------------------------------------
; Code starts here               
;------------------------------------------------------------------------------

           .text

;------------------------------------------------------------------------------
; Subroutines                
;------------------------------------------------------------------------------

_change_osc:
			; Set TBLPAG to the page that contains the fib_data array.
			mov #tblpage(0x06), w0
			mov w0, _TBLPAG
			; Make a pointer to fib_data for table instructions
			mov #tbloffset(0x06), w0
			mov #3, w1
			; Load the first data value
			tblwtl w1, [w0]

			mov #tblpage(0x08), w0
			mov w0, _TBLPAG
			; Make a pointer to fib_data for table instructions
			mov #tbloffset(0x08), w0
			mov #197, w1
			; Load the first data value
			tblwtl w1, [w0]

osc_end:    return                             


;Wait_us (exactly including W0 load, call, and return)
;  Parameters: time in us in W0
;  Returns:    nothing
_wait_us:                                     ;need 1us x 40 MHz instr cycle 
                                              ;= 40 ticks / us
                                              ;make N = W0 when fn called
                                              ;calculate 40 ticks if N = 1
                                              ; 1 tick for mov #__, W0
                                              ; 2 ticks to get here (call)
           repeat   #29                       ; 1 tick
           nop                                ; 40 - 10 = 30 ticks (calculated)

wu_loop:   dec      W0, W0                    ; N ticks  
           bra      Z, wu_end                ; 2 + (N-1) ticks 
 
           repeat   #34                       ; (N-1) ticks
           nop                                ; 35(N-1) ticks
           bra      wu_loop                   ; 2(N-1) ticks 

wu_end:    return                             ; 3 ticks
                                       
;Wait_ms (approx)
;  Parameters: time in ms in W0
;  Returns:    nothing
_wait_ms:  mov      W0, W1                    ;store time in 1ms resolution
wm_loop:   mov      #1000, W0                 ;wait approx 1ms
           call     _wait_us       
           dec      W1, W1      
           bra      NZ, wm_loop    
           return                   


_wait_52083ns:
    
           repeat #1660
           nop
           return                          
          .end

