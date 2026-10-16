/* fake_gd.c - a stand-in "GeometryDash.exe" for testing live mode without the real game.
 *
 * Its code contains the same instruction shapes pmsyncgd looks for:
 *   - two "other item" checks that vote for fake_isu (like isItemUnlocked's other callers)
 *   - two Music Unlocker checks (test al,al / xor al,1), wrapped as callable functions
 *   - a decoy Music Unlocker call to a different function (like toggleEnableItem)
 * It then waits for pmsyncgd to patch it in memory and checks the result from the inside.
 */
#include <stdio.h>
#include <string.h>
#include <windows.h>

int music_check_test(void);   /* 1 = unlocked */
int music_check_xor(void);    /* 1 = locked   */
extern unsigned char decoy_slot[], voter_slot[];

__asm__(
    ".text\n"
    "fake_isu:\n"
    "    xor %eax, %eax\n"            /* nobody owns anything in here */
    "    ret\n"
    "fake_toggle:\n"
    "    ret\n"

    ".globl music_check_test\n"
    "music_check_test:\n"
    "    .byte 0xBA,0x0C,0,0,0, 0x41,0xB8,0x11,0,0,0\n"
    "    call fake_isu\n"
    "    test %al, %al\n"
    "    setne %al\n"
    "    movzbl %al, %eax\n"
    "    ret\n"

    ".globl music_check_xor\n"
    "music_check_xor:\n"
    "    .byte 0xBA,0x0C,0,0,0, 0x41,0xB8,0x11,0,0,0\n"
    "    call fake_isu\n"
    "    xor $1, %al\n"
    "    movzbl %al, %eax\n"
    "    ret\n"

    /* never executed - just bytes for the scanner to see */
    "    .byte 0xBA,0x0C,0,0,0, 0x41,0xB8,0x10,0,0,0\n"
    ".globl voter_slot\n"
    "voter_slot:\n"
    "    call fake_isu\n"
    "    test %al, %al\n"
    "    .byte 0xBA,0x0C,0,0,0, 0x41,0xB8,0x05,0,0,0\n"
    "    call fake_isu\n"
    "    test %al, %al\n"
    "    .byte 0xBA,0x0C,0,0,0, 0x41,0xB8,0x11,0,0,0\n"
    ".globl decoy_slot\n"
    "decoy_slot:\n"
    "    call fake_toggle\n"
    "    mov (%rax), %rcx\n"
    "    ret\n");

int main(void)
{
    unsigned char decoy[5], voter[5];
    int i;

    memcpy(decoy, decoy_slot, 5);
    memcpy(voter, voter_slot, 5);
    printf("fake GD up: unlocked=%d locked=%d\n", music_check_test(), music_check_xor());
    fflush(stdout);

    for (i = 0; i < 300; i++) {   /* up to 30 s */
        if (music_check_test() == 1 && music_check_xor() == 0) {
            int untouched = !memcmp(decoy, decoy_slot, 5) && !memcmp(voter, voter_slot, 5);
            printf("patched: unlocked=1 locked=0, decoy/voter untouched=%d\n", untouched);
            return untouched ? 0 : 3;
        }
        Sleep(100);
    }
    printf("timed out waiting for the patch\n");
    return 1;
}
