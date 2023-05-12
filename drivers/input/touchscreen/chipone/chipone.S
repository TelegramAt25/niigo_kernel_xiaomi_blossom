/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/linkage.h>

#include <asm/alternative.h>
#include <asm/assembler.h>
#include <asm/cpufeature.h>

.data
data_ffffff80093198bf:
.byte 0x3c,0x49,0x3e,0x43,0x54,0x53,0x2d,0x20,0x43,0x68,0x69,0x70,0x6f,0x6e,0x65,0x20,0x54,0x44,0x44,0x49,0x20,0x54,0x50,0x44,0x20,0x64,0x72,0x69,0x76,0x65,0x72,0x20,0x25,0x73,0x0a,0x00
data_ffffff80093198e3:
.byte 0x76,0x31,0x2e,0x33,0x2e,0x31,0x00

.text

ENTRY(chipone_tpd_driver_init)
    .byte 0xf3,0x0f,0x1e,0xf8                                     /* ffffff800947841c; str x19, [sp, #-0x20]! */
    .byte 0xfd,0x7b,0x01,0xa9                                     /* ffffff8009478420; stp x29, x30, [sp, #0x10] */
    .byte 0xfd,0x43,0x00,0x91                                     /* ffffff8009478424; add x29, sp, #0x10 */
    adrp x1, data_ffffff80093198bf
    adrp x2, data_ffffff80093198e3
    #.byte 0x01,0xf5,0xff,0xb0                                     /* ffffff8009478428; adrp x1, #0xffffff8009319000 */
    #.byte 0x02,0xf5,0xff,0xb0                                     /* ffffff800947842c; adrp x2, #0xffffff8009319000 */
    add x1, x1, :lo12:data_ffffff80093198bf
    #.byte 0x21,0xfc,0x22,0x91                                     /* ffffff8009478430; add x1, x1, #0x8bf */
    add x2, x2, :lo12:data_ffffff80093198e3
    #.byte 0x42,0x8c,0x23,0x91                                     /* ffffff8009478434; add x2, x2, #0x8e3 */
    .byte 0xe0,0x03,0x1f,0x32                                     /* ffffff8009478438; orr w0, wzr, #2 */
    .byte 0xf3,0x03,0x1f,0x32                                     /* ffffff800947843c; orr w19, wzr, #2 */
    # .byte 0x85,0x3c,0xdb,0x97                                     /* ffffff8009478440; bl #0xffffff8008b47654 <cts_log> */
    bl tpd_get_dts_info
    # .byte 0x19,0xba,0xd9,0x97                                     /* ffffff8009478444; bl #0xffffff8008ae6ca8 <tpd_get_dts_info> */
    .byte 0xa8,0x6f,0x00,0xf0                                     /* ffffff8009478448; adrp x8, #0xffffff800a26f000 */
    .byte 0x09,0x29,0x4e,0xb9                                     /* ffffff800947844c; ldr w9, [x8, #0xe28] */
    .byte 0x3f,0x09,0x00,0x71                                     /* ffffff8009478450; cmp w9, #2 */
    .byte 0x8b,0x00,0x00,0x54                                     /* ffffff8009478454; b.lt #0xffffff8009478464 <chipone_tpd_driver_init+0x48> */
    .byte 0x3f,0x2d,0x00,0x71                                     /* ffffff8009478458; cmp w9, #0xb */
    .byte 0x6b,0x00,0x00,0x54                                     /* ffffff800947845c; b.lt #0xffffff8009478468 <chipone_tpd_driver_init+0x4c> */
    .byte 0x53,0x01,0x80,0x52                                     /* ffffff8009478460; movz w19, #0xa */
    .byte 0x13,0x29,0x0e,0xb9                                     /* ffffff8009478464; str w19, [x8, #0xe28] */
    .byte 0xe0,0x31,0x00,0xf0                                     /* ffffff8009478468; adrp x0, #0xffffff8009ab7000 */
    .byte 0x00,0x80,0x3b,0x91                                     /* ffffff800947846c; add x0, x0, #0xee0 */
    bl tpd_driver_add
    # .byte 0x42,0xbc,0xd9,0x97                                     /* ffffff8009478470; bl #0xffffff8008ae7578 <tpd_driver_add> */
    .byte 0x60,0x00,0xf8,0x37                                     /* ffffff8009478474; tbnz w0, #0x1f, #0xffffff8009478480 <chipone_tpd_driver_init+0x64> */
    .byte 0xf3,0x03,0x1f,0x2a                                     /* ffffff8009478478; mov w19, wzr */
    .byte 0x07,0x00,0x00,0x14                                     /* ffffff800947847c; b #0xffffff8009478498 <chipone_tpd_driver_init+0x7c> */
    .byte 0xf3,0x03,0x00,0x2a                                     /* ffffff8009478480; mov w19, w0 */
    .byte 0x01,0xf5,0xff,0xb0                                     /* ffffff8009478484; adrp x1, #0xffffff8009319000 */
    .byte 0x21,0xa8,0x23,0x91                                     /* ffffff8009478488; add x1, x1, #0x8ea */
    .byte 0xe0,0x03,0x1f,0x2a                                     /* ffffff800947848c; mov w0, wzr */
    .byte 0xe2,0x03,0x13,0x2a                                     /* ffffff8009478490; mov w2, w19 */
    .byte 0x70,0x3c,0xdb,0x97                                     /* ffffff8009478494; bl #0xffffff8008b47654 <cts_log> */
    .byte 0xfd,0x7b,0x41,0xa9                                     /* ffffff8009478498; ldp x29, x30, [sp, #0x10] */
    .byte 0xe0,0x03,0x13,0x2a                                     /* ffffff800947849c; mov w0, w19 */
    .byte 0xf3,0x07,0x42,0xf8                                     /* ffffff80094784a0; ldr x19, [sp], #0x20 */
    # data inside code section at 0xffffff80094784a4 -- 0xffffff80094784a8
    .byte 0xc0,0x03,0x5f,0xd6
ENDPROC(chipone_tpd_driver_init)
