/*
File System Checker
 Assumptions: 
 1.) All empty files are 4096 0's in them.
 2.) DeviceID =  20 (cause that's what it said HW 2)
 3.) Time in previous code was in seconds casue  and in this code it is in milliseconds and I have divided that by 1000 to approximate. 
 4.) To make matters simpler I had put in a , in the end of all the free block list files i.e. all my files in the fuesdata.1-26 ended in ,}
 5.) File location array looks like this {35,36,37,} checkout the , in the end
 */

import java.io.*;
import java.util.*;

public class fsck {

    public static void checkDeviceID() throws Exception {
        try (BufferedReader br = new BufferedReader(new FileReader("/fusedata/fusedata.0"))) {
            String line = br.readLine();
            if (line.contains("devId:20")) {
                System.out.println("devId:20 checked");
            } else {
                System.out.println("devId incorrect");
                System.exit(0);
            }
            if (Long.parseLong(line.substring(line.indexOf(":") + 1, line.indexOf(","))) < ((new Date()).getTime() / 1000)) {
                System.out.println("Root Block Creation Time checked");
            } else {
                System.out.println("Root Block Creation Time incorrect");
                System.exit(0);
            }
        } catch (Exception e) {
            System.out.println("File fusedata.0 not available either in the wrong place or not existing at all");
            System.exit(0);
        }
    }

    public static void validateFreeBlockList() throws Exception {
        int blocks[] = new int[10000];
        String emptyFile = "";
        for (int i = 0; i < 4096; i++) {
            emptyFile += "0";
        }
        for (int i = 1; i < 26; i++) {
            try (BufferedReader br = new BufferedReader(new FileReader("/fusedata/fusedata." + i))) {
                String line = br.readLine().substring(1);                
                while (line.contains(",")) {
                    blocks[Integer.parseInt(line.substring(0, line.indexOf(",")))] = 1;
                    line = line.substring(line.indexOf(",") + 1);
                }
            } catch (Exception e) {
                System.out.println("File fusedata." + i + "not available either in the wrong place or not existing at all");
                System.exit(0);
            }
        }
        for (int i = 26; i < blocks.length; i++) {
            if (blocks[i] == 0) {
                try (BufferedReader br = new BufferedReader(new FileReader("/fusedata/fusedata." + i))) {                    
                    if (emptyFile.equals(br.readLine())) {                   
                        System.out.println("Should Not be FREE error in fusedata." + i);
                        System.exit(0);
                    }
                } catch (Exception e) {
                    System.out.println("File fusedata." + i + "not available either in the wrong place or not existing at all");
                    System.exit(0);
                }
            } else {
                try (BufferedReader br = new BufferedReader(new FileReader("/fusedata/fusedata." + i))) {
                    if (!emptyFile.equals(br.readLine())) {
                        System.out.println("Should Be FREE error in fusedata." + i);
                        System.exit(0);
                    }
                } catch (Exception e) {
                    System.out.println("File fusedata." + i + "not available either in the wrong place or not existing at all");
                    System.exit(0);
                }
            }
        }
        System.out.println("Free Block list checked");
    }

    public static void checkFile(int block) throws Exception {
        try (BufferedReader br = new BufferedReader(new FileReader("/fusedata/fusedata." + block))) {
            String line = br.readLine();
            if (!(line.contains("size:") && line.contains("linkcount:") && line.contains("location:") && line.contains("indirect:"))) {
                System.out.println("Error in file format for file in fusedata." + block);
                System.exit(0);
            }
            line = line.substring(line.indexOf("size:") + 5);
            int size = Integer.parseInt(line.substring(0, line.indexOf(",")));
            line = line.substring(line.indexOf("linkcount:") + 10);
            int linkcount = Integer.parseInt(line.substring(0, line.indexOf(",")));
            line = line.substring(line.indexOf("atime:") + 6);
            if (Integer.parseInt(line.substring(0, line.indexOf(","))) > ((new Date()).getTime() / 1000)) {
                System.out.println("Error in atime in fusedata." + block);
                System.exit(0);
            }
            line = line.substring(line.indexOf("ctime:") + 6);
            if (Integer.parseInt(line.substring(0, line.indexOf(","))) > ((new Date()).getTime() / 1000)) {
                System.out.println("Error in ctime in fusedata." + block);
                System.exit(0);
            }
            line = line.substring(line.indexOf("mtime:") + 6);
            if (Integer.parseInt(line.substring(0, line.indexOf(","))) > ((new Date()).getTime() / 1000)) {
                System.out.println("Error in mtime in fusedata." + block);
                System.exit(0);
            }
            line = line.substring(line.indexOf("indirect:") + 9);
            int indirect = Integer.parseInt(line.substring(0, line.indexOf(",")));
            line = line.substring(line.indexOf("location:") + 9);
            int location = Integer.parseInt(line.substring(0, line.indexOf("}")));
            if (indirect == 1) {
                try (BufferedReader brs = new BufferedReader(new FileReader("/fusedata/fusedata." + location))) {
                    String lines = brs.readLine();
                    if (lines.charAt(0) != '{' && lines.charAt(lines.length() - 1) != '}') {
                        System.out.println("location is not array fusedata." + location);
                        System.exit(0);
                    }
                    for (int i = 1; i < lines.length() - 1; i++) {
                        if (!((lines.charAt(i) >= '0' && lines.charAt(i) <= '9') || lines.charAt(i) == ',')) {
                            System.out.println("location is not array fusedata." + location);
                            System.exit(0);
                        }
                    }
                    if(size > lines.length() - lines.replace(",", "").length() ){
                        System.out.println("Size Error in fusedata."+location+" in file fusedata."+block);
                        System.exit(0);
                    }                                    
                    
                } catch (Exception e) {
                    System.out.println("File not at location fusedata." + location);
                    System.exit(0);
                }
            } else {
                if (size != 0) {
                    System.out.println("size error array fusedata." + block);
                    System.exit(0);
                }
            }
        } catch (Exception e) {
            System.out.println("File not available either in the wrong place or not existing at all fusedata." + block);
            System.exit(0);
        }

    }

    public static void checkDir(int block) throws Exception {
        try (BufferedReader br = new BufferedReader(new FileReader("/fusedata/fusedata." + block))) {
            String tmp = "", line = br.readLine();            
            if (line.contains("d:.:")) {
                String currBlock = line.substring(line.indexOf("d:.:") + 4);
                currBlock = currBlock.substring(0, currBlock.indexOf(","));
                if (Integer.parseInt(currBlock) != block) {
                    System.out.println("Incorrect current directory '.' fusedata." + block);
                    System.exit(0);
                }
            } else {
                System.out.println("Incorrect current directory '.' fusedata." + block);
                System.exit(0);
            }
            if (line.contains("d:..:")) {
                String currBlock = line.substring(line.indexOf("d:..:") + 5);
                currBlock = currBlock.substring(0, currBlock.indexOf(","));
                try (BufferedReader brs = new BufferedReader(new FileReader("/fusedata/fusedata." + currBlock))) {
                    String root = brs.readLine();
                    if (!(root.contains(":" + currBlock + ",") || root.contains(":" + currBlock + "}"))) {
                        System.out.println("Incorrect Root directory '..' fusedata." + currBlock + " for fusedata." + block);
                        System.exit(0);
                    }
                } catch (Exception e) {
                    System.out.println("File not available either in the wrong place or not existing at all fusedata." + currBlock);
                    System.exit(0);
                }
            }
            String timings = line.substring(line.indexOf("atime:") + 6);
            if (Integer.parseInt(timings.substring(0, line.indexOf(","))) > ((new Date()).getTime() / 1000)) {
                System.out.println("Error in atime in fusedata." + block);
                System.exit(0);
            }
            timings = line.substring(line.indexOf("ctime:") + 6);
            if (Integer.parseInt(timings.substring(0, line.indexOf(","))) > ((new Date()).getTime() / 1000)) {
                System.out.println("Error in ctime in fusedata." + block);
                System.exit(0);
            }
            timings = line.substring(line.indexOf("mtime:") + 6);
            if (Integer.parseInt(timings.substring(0, line.indexOf(","))) > ((new Date()).getTime() / 1000)) {
                System.out.println("Error in mtime in fusedata." + block);
                System.exit(0);
            }

            String inode = line.substring(line.indexOf("filename_to_inode_dict:{") + 24);
            inode = inode.substring(inode.indexOf(",") + 1);
            inode = inode.substring(inode.indexOf(",") + 1);
            if (line.contains("linkcount:")) {
                String links = line.substring(line.indexOf("linkcount:") + 10);
                links = links.substring(0, links.indexOf(","));
                if ((inode.length() - inode.replace(",", "").length() + 3) != Integer.parseInt(links)) {
                    System.out.println("lincount doesn't march filename_to_inode_dict links in fusedata." + block);
                    System.exit(0);
                }
            } else {
                System.out.println("linkcount not found fusedata." + block);
                System.exit(0);
            }
            while (inode.length() > 2) {
                if (!inode.contains(",")) {
                    tmp = inode.substring(0, inode.indexOf("}"));
                    inode = inode.substring(inode.indexOf("}") + 1);
                } else {
                    tmp = inode.substring(0, inode.indexOf(","));
                    inode = inode.substring(inode.indexOf(",") + 1);
                }
                if (tmp.charAt(0) == 'd') {
                    tmp = tmp.substring(tmp.indexOf(":") + 1);
                    tmp = tmp.substring(tmp.indexOf(":") + 1);
                    checkDir(Integer.parseInt(tmp));
                } else {
                    tmp = tmp.substring(tmp.indexOf(":") + 1);
                    tmp = tmp.substring(tmp.indexOf(":") + 1);
                    checkFile(Integer.parseInt(tmp));
                }
            }
        } catch (Exception e) {
            System.out.println("File not available either in the wrong place or not existing at all fusedata." + block);
            System.exit(0);
        }

    }

    public static void main(String[] args) throws Exception {
        checkDeviceID();
        System.out.println("Checking Free Block list will take time ....");
        validateFreeBlockList();
        System.out.println("All Directories & Files checking...");
        checkDir(26);        
        System.out.println("All Directories & Files checked");
    }
}
