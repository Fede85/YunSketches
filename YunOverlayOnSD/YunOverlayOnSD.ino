#include <Bridge.h>
#include <Process.h>

#define OK 0
int dataPartitionSize = 700;
long lininoBaud = 250000;

void setup() {

  // Initialize the serial communication
  // and wait until the port is opened
  Serial.begin(115200);
  while (!Serial);

  // Initialize the Bridge
  Bridge.begin();

  Serial.print(F("Create an overlay on SD card. \nDo you want to proceed (yes/no)? "));
  readAnswerToProceed();

  Serial.print(F("SD card check: "));

  // check if the SD card is present
  Process ls;
  ls.begin("ls");
  ls.addParameter("/dev/sda");
  int exitCode = ls.run();

  if (exitCode != 0) {
    Serial.println(F("SD not present"));
    while (1);
  }

  Serial.println(F("OK\n"));

  Serial.print(F("Install softwares(yes/no)? "));
  readAnswerToProceed();

  Process opkg;

  // update the packages list
  exitCode = opkg.runShellCommand("opkg update");
  // if the exitCode of the process is OK the package has been installed correctly
  if (exitCode != OK) {
    Serial.println(F("err. with opkg, check internet connection"));
    while (1); // block the execution
  }
  exitCode = opkg.runShellCommand(F("opkg install e2fsprogs"));
  if (exitCode != OK) {
    Serial.println(F("err. installing e2fsprogs"));
    while (1); // block the execution
  }
  exitCode = opkg.runShellCommand(F("opkg install fdisk"));
  if (exitCode != OK) {
    Serial.println(F("err. installing fdisk"));
    while (1); // block the execution
  }

  Serial.println(F("OK\n"));

  // partitioning the SD
  Serial.print(F("proceed with SD partition (yes/no)? "));
  readAnswerToProceed();

  Process format;

  // unmount the SD card
  exitCode = format.runShellCommand(F("umount /dev/sda1"));
  if (exitCode == OK) {
    Serial.println(F("err. unable to unmout /dev/sda1"));
    while (1); // block the execution
  }



  // create the first partition
  dataPartitionSize = readPartitionSize();
  String firstPartition = "(echo d; echo n; echo p; echo 1; echo; echo +";
  firstPartition += dataPartitionSize;
  firstPartition += "M; echo w) | fdisk /dev/sda";
  exitCode = format.runShellCommand(firstPartition);
  printProcessOutput(format);
  if (exitCode != OK) {
    Serial.println(F("err. during 1st partition"));
    while (1); // block the execution
  }
  Serial.println(F("1st OK"));
  delay(100);



  // create the second partition
  exitCode = format.runShellCommand(F("(echo n; echo p; echo 2; echo; echo; echo w) | fdisk /dev/sda"));
  printProcessOutput(format);
  if (exitCode != OK) {
    Serial.println(F("err. during 2nd partition"));
    while (1); // block the execution
  }
  Serial.println(F("2nd OK"));
  delay(100);


  format.runShellCommand(F("umount /dev/sda*"));

  // format the first partition in the FAT32 format
  exitCode = format.runShellCommand(F("(echo t; echo 1; echo c; echo w) | fdisk /dev/sda"));
  printProcessOutput(format);
  if (exitCode != OK) {
    Serial.println(F("err. formatting to FAT32"));
    while (1); // block the execution
  }
  Serial.println(F("FAT32 OK"));
  delay(200);



  // format the second partition to Linux EXT4
  exitCode = format.runShellCommand("mkfs.ext4 /dev/sda2");
  printProcessOutput(format);
  if (exitCode != OK) {
    Serial.println(F("err. formatting to EXT4"));
    while (1); // block the execution
  }

  Serial.println(F("Partition created\n"));

  // modify fstab
  Serial.println(F("Configuring fstab file\n"));

  Process fstab;

  fstab.runShellCommand(F("uci add fstab mount"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].target=/overlay"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].device=/dev/sda2"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].fstype=ext4"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].enabled=1"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].enabled_fsck=0"));
  fstab.runShellCommand(F("uci set fstab.@mount[0].options=rw,sync,noatime"));
  fstab.runShellCommand(F("uci commit"));

  // reboot
  Serial.println(F("Now rebooting to make the changes effective"));
  Serial.flush();

  Serial1.begin(lininoBaud); // open serial connection to Linino
  delay(100);

  Process reboot;
  reboot.runShellCommand(F("reboot"));
}

void loop() {
  // copy from virtual serial line to uart and vice versa
  if (Serial.available()) {           // got anything from USB-Serial?
    char c = (char)Serial.read();     // read from USB-serial
    Serial1.write(c);
  }
  if (Serial1.available()) {          // got anything from Linino?
    char c = (char)Serial1.read();    // read from Linino
    Serial.write(c);                  // write to USB-serial
  }
}


void readAnswerToProceed()
{
  // wait until somethig arrive on serial port
  while (Serial.available() == 0);

  String answer = Serial.readStringUntil('\n');

  Serial.println(answer);
  if (answer != "yes")
  {
    Serial.println(F("\nGoodbye"));
    while (1); // do nothing more forever
  }
}

int readPartitionSize()
{
  // wait until somethig arrive on serial port
  int i = 0;
  while (!i)
  {
    Serial.print(F("Enter the size of the data partition in MB: "));
    // wait until somethig arrive on serial port
    while (Serial.available() == 0);

    String answer = Serial.readStringUntil('\n');
    i = answer.toInt();
    Serial.println(i);
    if (!i)
      Serial.println(F("invalid input, retry"));
  }
  return i;
}

void printProcessOutput(Process p)
{
  while (p.running());

  while (p.available() > 0) {
    char c = p.read();
    Serial.print(c);
  }
  Serial.flush();
}
