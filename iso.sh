mkdir -p iso/boot/limine

cp limine.conf vendor/limine/limine-bios.sys vendor/limine/limine-bios-cd.bin vendor/limine/limine-uefi-cd.bin iso/boot/limine
cp build/cosmos-os iso/boot

cp build/subprojects/shell/shell iso

xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label iso -o cosmos-os.iso

./vendor/limine/limine bios-install cosmos-os.iso
