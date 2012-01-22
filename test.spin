{
        test.spin
}
CON
'    _clkmode = rcslow
'    _clkmode = xinput
    _clkmode = xtal1 + pll16x
    _xinfreq = 5_000_000

        ClkPin = 10
        OtherPin = ClkPin+1
        SomeFloat = 1.543
        Test1 = 1+4/2*(5+(20/5))
        Test2 = 1.0/3.0+32.0
        Test3 = ((10/5)*32)&64
   ACK           = 0            ' I2C Acknowledge
   NAK           = 1            ' I2C No Acknowledge
   Xmit          = 0            ' I2C Direction Transmit
   Recv          = 1            ' I2C Direction Receive

   CMD_START     = 1            ' Issue a start bit
   CMD_STOP      = 2            ' Issue a stop bit
   CMD_READ      = 3            ' Transmit a byte to the I2C bus
   CMD_WRITE     = 4            ' Read a byte from the I2C bus
   CMD_INIT      = 5            ' Initialize the I2C bus
   CMD_READPAGE  = 6            ' Read one or more bytes in the page mode
   CMD_WRITEPAGE = 7            ' Write one or more bytes in the page mode
   
   DELAY_CYCLES  = 52           ' I2C Delay time.  Must be between 12 and 511

DAT
  cognum long 0
  cmdbuf long 0, 0

OBJ
 pst : "Parallax Serial Terminal"
 math : "FloatMath"
 pst2 : "Parallax Serial Terminal"

VAR
 long testLong
 byte testByte
 word testWord
 byte testByteArray[5]
 long testLongArray[3]
 word testWord2, testWord3
 long testLong2, testLong3
 byte testByte2, testByte3, testByte4

PUB main(param1, param2, param3) : retVal | local1, local2[6], local3, local4
 pst.Start(9600)
 pst2.Start(19200)
 'pst2[1].Start(19200)
 local1 := 10
 local2[0] := 20
 local2[5] := 25
 local3 := 3
 retVal := param1 + param2 + (local1 * local2[5]) / local3

PRI myPrivate(priParam1) | priLocal1
 result := priParam1 * 42

PRI myPrivate2
 result := 42

PUB myPub
 ' a comment
 waitcnt(80_000_000)

DAT
 'FILE "datFile1.dat"
 'FILE "datFile2.dat"

DEV
 PRECOMPILE "file1.pre"
 ARCHIVE "file1.arc"
 PRECOMPILE "file2.pre"

DEV
 ARCHIVE "file2.arc"
 PRECOMPILE "file2.pre"
 ARCHIVE "file3.arc"

DAT

'***********************************
'* Assembly language i2c driver    *
'***********************************

                        org
'
'
' Entry
' Wait for a non-zero command and process                        
cmdloop                 rdlong  t1, par            wz
        if_z            jmp     #cmdloop
                        mov     parm1, par
                        add     parm1, #4
                        rdlong  parm1, parm1    ' Get the address of the parameter list

                        rdlong  t2, parm1       ' SCL is always the first parameter
                        add     parm1, #4       ' Point to the next parameter
                        mov     scl_bit,#1
                        shl     scl_bit,t2
                        mov     sda_bit, scl_bit
                        shl     sda_bit, #1
                        
                        cmp     t1, #CMD_READPAGE  wz
        if_z            jmp     #ReadPage1
                        cmp     t1, #CMD_WRITEPAGE wz
        if_z            jmp     #WritePage1
                        cmp     t1, #CMD_READ      wz          
        if_z            jmp     #read_byte
                        cmp     t1, #CMD_WRITE     wz          
        if_z            jmp     #write_byte
                        cmp     t1, #CMD_START     wz          
        if_z            jmp     #start1
                        cmp     t1, #CMD_STOP      wz          
        if_z            jmp     #stop1
                        cmp     t1, #CMD_INIT      wz          
        if_z            jmp     #initialize1
                        neg     parm1, #1

ReturnParm              mov     t1, par
                        add     t1, #4
                        wrlong  parm1, t1
signal_ready            mov     t1, #0
                        wrbyte  t1, par
                        jmp     #cmdloop

ReadPage1               call    #ReadPageFunc
                        jmp     #ReturnParm
WritePage1              call    #WritePageFunc
                        jmp     #ReturnParm
read_byte               rdlong  parm1, parm1
                        call    #readbytefunc
                        jmp     #ReturnParm
write_byte              rdlong  parm1, parm1
                        call    #writebytefunc
                        jmp     #ReturnParm
start1                  call    #StartFunc
                        jmp     #ReturnParm
stop1                   call    #StopFunc
                        jmp     #ReturnParm
initialize1             call    #InitializeFunc
                        jmp     #ReturnParm

'' This routine reads a byte and sends the ACK bit.  It assumes the clock
'' and data lines have been low for at least the minimum low clock time.
'' It exits with the clock and data low for the minimum low clock time.                        
readbytefunc            mov     ackbit1, parm1 ' Get the ACK bit
                        mov     data1, #0     ' Initialize data byte to zero
                        andn    dira, sda_bit ' Set SDA as input
                        call    #delay
                        mov     count1, #8    ' Set loop count for 8

:loop                   call    #delay
                        or      outa, scl_bit ' Set SCL HIGH
                        call    #delay
                        shl     data1, #1     ' data byte left one bit
                        test    sda_bit, ina    wz
        if_nz           or      data1, #1     ' Set LSB if input bit is HIGH
                        andn    outa, scl_bit ' Set SCL LOW
                        call    #delay
                        djnz    count1, #:loop

                        cmp     ackbit1, #0     wz
        if_z            andn    outa, sda_bit ' Set SDA LOW if ACK
        if_nz           or      outa, sda_bit ' Set SDA HIGH if NAK
                        or      dira, sda_bit ' Set SDA as output
                        call    #delay
                        or      outa, scl_bit ' Set SCL HIGH
                        call    #delay
                        andn    outa, scl_bit ' Set SCL LOW
                        call    #delay
                        mov     parm1, data1  ' Return the data byte
readbytefunc_ret        ret

'' This routine writes a byte and reads the ACK bit.  It assumes that the clock
'' and data are set as outputs, and the clock has been low for at least half the
'' minimum low clock time.  It exits with the clock and data set as outputs, and
'' with the clock low for half the minimum low clock time.                        
writebytefunc           mov     data1, parm1  ' Get the data byte
                        mov     count1, #8    ' Set loop count for 8 bits

:loop                   shl     data1, #1     ' Shift left one bit
                        test    data1, #$100    wz ' Check MSB
        if_z            andn    outa, sda_bit ' Set SDA LOW if zero
        if_nz           or      outa, sda_bit ' Set SDA HIGH if not zero
                        call    #delay
                        or      outa, scl_bit ' Set SCL HIGH
                        call    #delay
                        andn    outa, scl_bit ' Set SCL LOW
                        call    #delay
                        djnz    count1, #:loop

                        andn    dira, sda_bit ' Set SDA as input
                        call    #delay
                        or      outa, scl_bit ' Set SDA HIGH
                        call    #delay
                        test    sda_bit, ina    wz ' Check SDA input
        if_z            mov     ackbit1, #0   ' Set to zero if LOW
        if_nz           mov     ackbit1, #1   ' Set to one if HIGH
                        andn    outa, scl_bit ' Set SCL LOW
                        call    #delay
                        or      dira, sda_bit ' Set SDA as output
                        mov     parm1, ackbit1 ' Return the ack bit
writebytefunc_ret       ret

'' This routine transmits the stop sequence, which consists of the data line
'' going from low to high while the clock is high.  It assumes that data and
'' clock are set as outputs, and the clock has been low for half the minimum
'' low clock time.  It exits with the clock and data floating high for the
'' minimum  high clock time.
stopfunc                andn    outa, sda_bit ' Set SDA LOW
                        call    #delay
                        or      outa, scl_bit ' Set SCL HIGH
                        call    #delay
                        or      outa, sda_bit ' Set SDA HIGH
                        call    #delay
                        andn    dira, scl_bit ' Float SCL HIGH
                        andn    dira, sda_bit ' Float SDA HIGH
stopfunc_ret            ret

'' This routine transmits the start sequence, which consists of the data line
'' going from high to low while the clock is high.  It assumes that the clock
'' and data were floating high for the minimum high clock time, and it exits
'' with the clock and data low for half the minimum low clock time.
startfunc               or      outa, sda_bit ' Set SDA HIGH
                        or      dira, sda_bit ' Set SDA as output
                        call    #delay
                        or      outa, scl_bit ' Set SCL HIGH
                        or      dira, scl_bit ' Set SCL as output
                        call    #delay
                        andn    outa, sda_bit ' Set SDA LOW
                        call    #delay
                        andn    outa, scl_bit ' Set SCL LOW
                        call    #delay
startfunc_ret           ret

'' This routine puts the I2C bus in a known state.  It issues up to nine clock
'' pulses waiting for the input to be in a high state.  It exits with the clock
'' driven high and the data floating in the high state for the minimum high
'' clock time.
initializefunc          andn    dira, sda_bit ' Set SDA as input
                        or      outa, scl_bit ' Set SCL HIGH
                        or      dira, scl_bit ' Set SCL as output
                        call    #delay
                        mov     count1, #9    ' Set for up to 9 loops
:loop                   andn    outa, scl_bit ' Set SCL LOW
                        call    #delay
                        call    #delay
                        or      outa, scl_bit ' Set SCL HIGH
                        call    #delay
                        test    sda_bit, ina    wz
        if_nz           jmp     #initializefunc_ret ' Quit if input is HIGH
                        djnz    count1, #:loop
initializefunc_ret      ret                   ' Quit after nine clocks

'' This routine delays for the minimum high clock time, or half the minimum
'' low clock time.  This delay routine is also used for the setup and hold
'' times for the start and stop signals, as well as the output data changes.
delay                   mov     delaycnt, cnt
                        add     delaycnt, #DELAY_CYCLES
                        waitcnt delaycnt, #0
delay_ret               ret

'PUB ReadPage(SCL, devSel, addrReg, dataPtr, count) : ackbit
readpagefunc            rdlong  devsel1, parm1
                        add     parm1, #4
                        rdlong  addrreg1, parm1
                        add     parm1, #4
                        rdlong  dataptr1, parm1
                        add     parm1, #4
                        rdlong  count2, parm1

'' Read in a block of i2c data.  Device select code is devSel.  Device starting
'' address is addrReg.  Data address is at dataPtr.  Number of bytes is count.
'' The device select code is modified using the upper 3 bits of the 19 bit addrReg.
'' Return zero if no errors or the acknowledge bits if an error occurred.
'   devSel |= addrReg >> 15 & %1110
                        mov     t1, addrreg1
                        shr     t1, #15
                        and     t1, #%1110
                        or      devsel1, t1
'   Start(SCL)                          ' Select the device & send address
                        call    #startfunc
'   ackbit := Write(SCL, devSel | Xmit)
                        mov     parm1, devsel1
                        or      parm1, #Xmit
                        call    #writebytefunc
                        mov     ackbit2, parm1
'   ackbit := (ackbit << 1) | Write(SCL, addrReg >> 8 & $FF)
                        mov     parm1, addrreg1
                        shr     parm1, #8
                        and     parm1, #$ff
                        call    #writebytefunc
                        shl     ackbit2, #1
                        or      ackbit2, parm1
'   ackbit := (ackbit << 1) | Write(SCL, addrReg & $FF)          
                        mov     parm1, addrreg1
                        and     parm1, #$ff
                        call    #writebytefunc
                        shl     ackbit2, #1
                        or      ackbit2, parm1
'   Start(SCL)                          ' Reselect the device for reading
                        call    #startfunc
'   ackbit := (ackbit << 1) | Write(SCL, devSel | Recv)
                        mov     parm1, devsel1
                        or      parm1, #Recv
                        call    #writebytefunc
                        shl     ackbit2, #1
                        or      ackbit2, parm1
'   repeat count - 1
'      byte[dataPtr++] := Read(SCL, ACK)
'   byte[dataPtr++] := Read(SCL, NAK)
:loop                   cmp     count2, #1 wz
        if_z            mov     parm1, #NAK
        if_nz           mov     parm1, #ACK
                        call    #readbytefunc
                        wrbyte  parm1, dataptr1
                        add     dataptr1, #1
                        djnz    count2, #:loop

'   Stop(SCL)
                        call    #stopfunc
'   return ackbit
                        mov     parm1, ackbit2
readpagefunc_ret        ret

'PUB WritePage(SCL, devSel, addrReg, dataPtr, count) : ackbit
writepagefunc           rdlong  devsel1, parm1
                        add     parm1, #4
                        rdlong  addrreg1, parm1
                        add     parm1, #4
                        rdlong  dataptr1, parm1
                        add     parm1, #4
                        rdlong  count2, parm1

'' Write out a block of i2c data.  Device select code is devSel.  Device starting
'' address is addrReg.  Data address is at dataPtr.  Number of bytes is count.
'' The device select code is modified using the upper 3 bits of the 19 bit addrReg.
'' Most devices have a page size of at least 32 bytes, some as large as 256 bytes.
'' Return zero if no errors or the acknowledge bits if an error occurred.  If
'' more than 31 bytes are transmitted, the sign bit is "sticky" and is the
'' logical "or" of the acknowledge bits of any bytes past the 31st.
'   devSel |= addrReg >> 15 & %1110
                        mov     t1, addrreg1
                        shr     t1, #15
                        and     t1, #%1110
                        or      devsel1, t1
'   Start(SCL)                          ' Select the device & send address
                        call    #startfunc
'   ackbit := Write(SCL, devSel | Xmit)
                        mov     parm1, devsel1
                        or      parm1, #Xmit
                        call    #writebytefunc
                        mov     ackbit2, parm1
'   ackbit := (ackbit << 1) | Write(SCL, addrReg >> 8 & $FF)
                        mov     parm1, addrreg1
                        shr     parm1, #8
                        and     parm1, #$ff
                        call    #writebytefunc
                        shl     ackbit2, #1
                        or      ackbit2, parm1
'   ackbit := (ackbit << 1) | Write(SCL, addrReg & $FF)          
                        mov     parm1, addrreg1
                        and     parm1, #$ff
                        call    #writebytefunc
                        shl     ackbit2, #1
                        or      ackbit2, parm1
'   repeat count                        ' Now send the data
'      ackbit := ackbit << 1 | ackbit & $80000000 ' "Sticky" sign bit         
'      ackbit |= Write(SCL, byte[dataPtr++])
:loop                   shl     ackbit2, #1 wc
        if_c            or      ackbit2, signbit
                        rdbyte  parm1, dataptr1
                        add     dataptr1, #1
                        call    #writebytefunc
                        or      ackbit2, parm1
                        djnz    count2, #:loop
'   Stop(SCL)
                        call    #stopfunc
'   return ackbit
                        mov     parm1, ackbit2
writepagefunc_ret       ret

signbit         long    $80000000
scl_bit         res     1
sda_bit         res     1
count1          res     1
t1              res     1
t2              res     1
data1           res     1
ackbit1         res     1
delaycnt        res     1
parm1           res     1
devsel1         res     1
addrreg1        res     1
dataptr1        res     1
count2          res     1
ackbit2         res     1