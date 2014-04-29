#include <Process.h>

#define DEBUG 0
#define SUCCESSFUL_EXIT_CODE 0

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.print(F("This sketch will format your uSD card and use it as additional disk space for your Arduino Yun.\nPlease ensure you have ONLY your uSD card plugged in: no pen drives, hard drives or whatever.\nDo you wish to proceed (yes/no)?"));
  expectYesBeforeProceeding();

  Serial.println(F("Starting Bridge..."));

  Bridge.begin();

  haltIfSDAlreadyOnOverlay();

  haltIfSDCardIsNotPresent();

  installSoftwares();

  partitionAndFormatSDCard();

  createArduinoFolder();

  enableExtRoot();

  Serial.print(F("\nWe are done! Yeah! Now unplug and replug the Arduino Yun to your computer in order to make the changes effective."));
}

void loop() {
  // This turns the sketch into a YunSerialMonitor
  if (Serial.available()) {
    char c = (char)Serial.read();
    Serial1.write(c);
  }
  if (Serial1.available()) {
    char c = (char)Serial1.read();
    Serial.write(c);
  }
}

void halt() {
  Serial.flush();
  while (true);
}

void expectYesBeforeProceeding() {
  Serial.flush();

  while (!Serial.available());

  String answer = Serial.readStringUntil('\n');

  Serial.print(F(" "));
  Serial.println(answer);
  if (answer != "yes") {
    Serial.println(F("\nGoodbye"));
    halt();
  }
}

int readPartitionSize() {
  int partitionSize = 0;
  while (!partitionSize)
  {
    Serial.print(F("Enter the size of the data partition in MB: "));
    while (Serial.available() == 0);

    String answer = Serial.readStringUntil('\n');
    partitionSize = answer.toInt();
    Serial.println(partitionSize);
    if (!partitionSize)
      Serial.println(F("Invalid input, retry"));
  }
  return partitionSize;
}

void debugProcess(Process p) {
  #if DEBUG == 1
  while (p.running());

  while (p.available() > 0) {
    char c = p.read();
    Serial.print(c);
  }
  Serial.flush();
  #endif
}

void haltIfSDAlreadyOnOverlay() {
  Process grep;

  grep.runShellCommand(F("mount | grep ^/dev/sda | grep 'on /overlay'"));
  String output = grep.readString();
  if (output != "") {
    Serial.println(F("uSD card is already used as additional Arduino Yun disk space. Nothing to do."));
    halt();
  }
}

void haltIfSDCardIsNotPresent() {
  Process ls;
  int exitCode = ls.runShellCommand("ls /mnt/sda1");

  if (exitCode != 0) {
    Serial.println(F("The uSD card is not available"));
    halt();
  }
}

void installSoftwares() {
  Serial.print(F("Ready to install utility softwares. Please ensure your Arduino Yun is connected to internet. Ready to proceed (yes/no)?"));
  expectYesBeforeProceeding();

  Serial.println(F("Updating software list..."));

  Process opkg;

  // update the packages list
  int exitCode = opkg.runShellCommand("opkg update");
  // if the exitCode of the process is OK the package has been installed correctly
  if (exitCode != SUCCESSFUL_EXIT_CODE) {
    Serial.println(F("err. with opkg, check internet connection"));
    debugProcess(opkg);
    halt();
  }
  Serial.println(F("Software list updated. Installing software..."));

  // install the utility to format in EXT4
  exitCode = opkg.runShellCommand(F("opkg install e2fsprogs mkdosfs fdisk"));
  if (exitCode != SUCCESSFUL_EXIT_CODE) {
    Serial.println(F("err. installing e2fsprogs mkdosfs fdisk"));
    debugProcess(opkg);
    halt();
  }
  Serial.println(F("e2fsprogs mkdosfs fdisk installed"));
  Serial.println();
}

void partitionAndFormatSDCard() {
  Serial.print(F("Proceed with partitioning uSD card (yes/no)?"));
  expectYesBeforeProceeding();

  unmount();

  Process format;

  //clears partition table
  format.runShellCommand("dd if=/dev/zero of=/dev/sda bs=4096 count=10");
  debugProcess(format);

  // create the first partition
  int dataPartitionSize = readPartitionSize();
  String firstPartition = "(echo n; echo p; echo 1; echo; echo +";
  firstPartition += dataPartitionSize;
  firstPartition += "M; echo w) | fdisk /dev/sda";
  format.runShellCommand(firstPartition);
  debugProcess(format);

  unmount();

  // create the second partition
  format.runShellCommand(F("(echo n; echo p; echo 2; echo; echo; echo w) | fdisk /dev/sda"));
  debugProcess(format);

  unmount();

  // specify first partition is FAT32
  format.runShellCommand(F("(echo t; echo 1; echo c; echo w) | fdisk /dev/sda"));

  unmount();

  delay(5000);

  unmount();

  // format the first partition to FAT32
  int exitCode = format.runShellCommand(F("mkfs.vfat /dev/sda1"));
  debugProcess(format);
  if (exitCode != SUCCESSFUL_EXIT_CODE) {
    Serial.println(F("\nerr. formatting to FAT32"));
    halt();
  }
  delay(100);

  // format the second partition to Linux EXT4
  exitCode = format.runShellCommand(F("mkfs.ext4 /dev/sda2"));
  debugProcess(format);
  if (exitCode != SUCCESSFUL_EXIT_CODE) {
    Serial.println(F("\nerr. formatting to EXT4"));
    halt();
  }

  Serial.println(F("\nuSD card correctly partitioned"));
}

void createArduinoFolder() {
  Process folder;

  folder.runShellCommand(F("mkdir -p /mnt/sda1"));
  folder.runShellCommand(F("mount /dev/sda1 /mnt/sda1"));
  folder.runShellCommand(F("mkdir -p /mnt/sda1/arduino/www"));

  unmount();
}

void unmount() {
  Process format;
  format.runShellCommand(F("umount /dev/sda?"));
  debugProcess(format);
  format.runShellCommand(F("rm -rf /mnt/sda?"));
  debugProcess(format);
}

void enableExtRoot() {
  Serial.print(F("\nEnabling uSD as additional disk space... "));

  Process fstab;

  fstab.runShellCommand(F("uci add fstab mount"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].target=/overlay"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].device=/dev/sda2"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].fstype=ext4"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].enabled=1"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].enabled_fsck=0"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].options=rw,sync,noatime,nodiratime"));
  fstab.runShellCommand(F("uci commit"));

  Serial.println(F("enabled"));
}

