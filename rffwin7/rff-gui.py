#!/usr/bin/python

## Rodent Face Finder GUI, Version 0.1.7, July 7, 2011
## Copyright 2011 Boston Biomedical Research Institute
## See LICENSE.TXT for license details
## Author: Oliver King, king@bbri.org

import sys
import os
import re
import math
import subprocess
import shlex
import string

# import Tkinter
from Tkinter import *
from tkFileDialog import askopenfilename
import tkMessageBox
import pickle ## use configparser instead?

root = Tk()   
root.title('Rodent Face Finder')
root.configure(background='white')

################

## global variables

gui_version_string = "Rodent Face Finder GUI, version 0.1.7, July 7, 2011"

sp2 = -1        ## suprocess run directly (not from queue)
spq = [-1,-1,-1,-1,-1,-1,-1,-1,-1] ## vector of queue subprocesses
max_jobs = 1
num_active = 0
stop_queue = 0 ## hack to stop queue

video_stem = ""   ## extension stripped, spaces changed to underscores
cage_num = 1      ## 1 for left, 2 for right

outstem_left = ""
outstem_right = ""

binaryname = ""

## is True, show warnings about directories
warnVar = BooleanVar()
warnVar.set(True)

## if True, last warning was dismissed with "continue" button
warnReturn = BooleanVar()
warnReturn.set(True)

## should do this with lambda callback instead!
def warnStop():
   warnReturn.set(False)
   ## set focus to entry2L or entry2R?
   warn.destroy()

def warnContinue():
    warnReturn.set(True)
    warn.destroy()


## check whether new jobs has same output path as job already in queue,
##  which could cause results to be intermingled:
def checkQueuePaths(new_command):
    overlap_count = 0
    oflagstart = new_command.find(' -o')
    oflagend = new_command.find(' ',oflagstart+4)            
    new_path = new_command[(oflagstart+4):oflagend].strip()  
    for i in range(0,len(queue_list)):
        old_command = queue_list[i]['analysis_command']
        if (old_command == new_command):
            text1.insert(END,"Warning: new command is exact duplicate of previously queued command.\n")
        oflagstart = old_command.find(' -o')
        oflagend = old_command.find(' ',oflagstart+4)            
        old_path = old_command[(oflagstart+4):oflagend].strip()
        if (old_path == new_path):
            overlap_count += 1
    # print "New command has same output path as " + str(overlap_count) + " previously queued commands."
    if (overlap_count > 0):
        text1.insert(END,"Warning: new command has same output path as " + str(overlap_count) + " previously queued commands.\n")
        text1.yview(END)
    return overlap_count


######################
def updateQueueColors():
    overlap_count = 0
    for j in range(0,len(queue_list)):
        queue_list[j]['oc'] = 0
    for j in range(0,len(queue_list)-1):
        new_command = queue_list[j]['analysis_command']
        oflagstart = new_command.find(' -o')
        oflagend = new_command.find(' ',oflagstart+4)            
        new_path = new_command[(oflagstart+4):oflagend].strip()  
        for i in range(j+1,len(queue_list)):
            old_command = queue_list[i]['analysis_command']
            oflagstart = old_command.find(' -o')
            oflagend = old_command.find(' ',oflagstart+4)            
            old_path = old_command[(oflagstart+4):oflagend].strip()
            if (old_path == new_path):
                queue_list[i]['oc'] += 1
                overlap_count += 1
        # print "New command has same output path as " + str(overlap_count) + " previously queued commands."
    queuebox.delete(0,END) ## overly drastic! could do this incrementally   
    for j in range(0,len(queue_list)):
        queuebox.insert(END, queue_list[j]['analysis_command'])
        if (queue_list[j]['status']=='running'):
            queuebox.itemconfig(END, bg='lightgreen')
        elif (queue_list[j]['status']=='completed'):
            queuebox.itemconfig(END, bg='pink')
        elif (queue_list[j]['status']=='killed'):
            queuebox.itemconfig(END, bg='grey')
        elif (queue_list[j]['status']=='pending'):
            queuebox.itemconfig(END, bg='yellow')
        ## else white, shouldn't happen
            
        if (queue_list[j]['oc'] > 0):
            queuebox.itemconfig(END, fg='red')
        else:
            queuebox.itemconfig(END, fg='black')
    return overlap_count

###################
## check whether any of the jobs in the queue have the same output
## path, which could cause results to be intermingled:
def checkAllQueuePaths():
    overlap_count = updateQueueColors()
    if (overlap_count > 0):
        warningString = "Warning: there are " + str(overlap_count) + " overlaps in output paths of queued commands. "
        warningString +="This may cause output images to be overwritten or intermingled.\n"
        text1.insert(END,warningString)
        text1.yview(END)
        warningString +="Continue anyway?\n"
        answer = tkMessageBox.askquestion("Warning", warningString, icon='warning')
        print answer
        if (answer == "yes"):
           overlap_count = 0
        else:
           pass
           ## put in edit mode? Or color-code the offending jobs?
    return overlap_count

###############################


def chooseVideo():
    ## global video_file_name
    video_file_name = askopenfilename(filetypes=[('avi, mpg, mts', 
                                                  '*.avi *.AVI *.mpg *.MPG *.mpeg *.MPEG *.mts *.MTS'),
                                                 ('all files', '*')])
    ## print 'Video file path: ' + video_file_name
    entry1.delete(0, END)
    entry1.insert(0, video_file_name)
    ## entry2.config(state=NORMAL)
    ## curstem = entry2.get()
    ## if (len(curstem) >= 0):
    ##    entry2.delete(0, END) ## not really necessary here
    curstem = video_file_name
    slash1 = curstem.rfind('\\')+1
    slash2 = curstem.rfind('/')+1
    slash = max(max(slash1,slash2),0)
    dot = curstem.rfind('.')
    if (dot < slash):
        dot = len(curstem)
    curstem = curstem[slash:dot]
    stemchunks = curstem.split('&',1); ## all token before ampersand
    leftID = stemchunks[0].strip();
    if (len(stemchunks)>1):
        rightID = stemchunks[1].strip()
        rightID = rightID.split()[0] ## just first token after ampersand?
        ## add initial characters from left ID, if right ID starts with number
        if (re.search("^\d", rightID) and not re.search("^\d", leftID)):
            if re.search("\d", leftID):
                rightID = leftID[0:re.search("\d",leftID).start()] + rightID
    else:
        rightID = leftID + "_right"
        leftID = leftID + "_left"
        
    curstem = curstem.replace(' ', '_')
    curstem = curstem.replace('(', '')
    curstem = curstem.replace(')', '')
    curstem = curstem.replace('<', '')
    curstem = curstem.replace('>', '')
    curstem = curstem.replace('&', '')
    
    rightID = rightID.replace(' ', '_')
    leftID = leftID.replace(' ', '_')
   
    global video_stem
    video_stem = curstem.strip()
    # entry2.delete(0,END)
    entry2L.delete(0,END)
    entry2R.delete(0,END)
    # entry2.insert(0,curstem)
    entry2L.insert(0,leftID)
    entry2R.insert(0,rightID)

def checkOutstem(outstem):
    ## does this put in a null character? gave error.
    # outstem = outstem.replace('<video_name>',video_stem)

    vali_stem = outstem;
    vali_stem = vali_stem.replace('<side>','')
    vali_stem = vali_stem.replace('<video_name>','');
    vali_stem = vali_stem.replace('<mouse_ID>','');
    vali_stem = vali_stem.replace('<img_option>','');
    vali_stem = vali_stem.replace('<frame_number>','');
    if ((vali_stem.find('>') >= 0) or (vali_stem.find('<') >= 0)):
        warningString = "invalid use of angled brackets in template for image names: only <video_name>, <mouse_ID>, <img_option>, <frame_number> and <side> are allowed.\n"
        text1.insert(END,warningString)
        text1.yview(END)
        tkMessageBox.showerror("Error", warningString, icon="error")
        return None  
    slash1 = outstem.rfind('\\')+1
    slash2 = outstem.rfind('/')+1
    slash = max(max(slash1,slash2),0)
    if (slash > 0):
        for img_opt in ['whole','zone','face','motion','whole_clean','zone_clean','face_clean']:
            if ( ((img_opt =='whole') and ((var4a.get()!='w') or (var5a.get()!='a')))
                or ((img_opt =='zone') and ((var4b.get()!='j') or (var5a.get()!='a')))
                or ((img_opt =='face') and ((var4c.get()!='f') or (var5a.get()!='a')))
                or ((img_opt =='whole_clean') and ((var4a.get()!='w') or (var5b.get()!='n')))
                or ((img_opt =='zone_clean') and ((var4b.get()!='j') or (var5b.get()!='n')))
                or ((img_opt =='face_clean') and ((var4c.get()!='f') or (var5b.get()!='n')))
                or ((img_opt =='motion') and (var5c.get()!='x'))):
                continue
            outpath = outstem[0:slash].replace('<img_option>', img_opt)
            if not os.path.isdir(outpath):
                ## print "attempting to create directory " + outpath + "\n"
                os.makedirs(outpath)
                if os.path.isdir(outpath):
                    # print "success\n"
                    text1.insert(END, "Directory " + outpath + " created.\n")
                else:
                    # Use pop-up here? And put images in top-level directory
                    text1.insert(END, "Failed to create directory " + outpath + ".\n")
                    text1.insert(END, "Check for illegal characters and file permissions " + outstem + ".\n")
                    text1.insert(END, "Using top-level-directory for output.\n")
                    outstem = outstem[(slash+1):len(outstem)] ## off by 1? 
            else:  ## check if any files with same stem exists. if so, pop up warning message
                ## offer a "don't warn me again" option?
                contents = os.listdir(outpath)
                has_pngs = False
                for fname in contents:
                    if fname.endswith("png"):
                        has_pngs = True
                        break
                if (has_pngs):
                    warningString = "Directory " + outpath + " already exists and contains images."
                    warningString += " Older images could be overwritten, or mixed in with newer images.\n"
                    text1.insert(END, warningString)
                    if (warnVar.get()):
                        global warn
                        warn = Toplevel()
                        warn.title("Warning")
                        wmsg = Message(warn, text=warningString, aspect=400, justify=CENTER)
                        ## wmsg = Message(warn, text=warningString, width=80, justify=CENTER) ## width not working on Mac 
                        wmsg.grid(row=0, column=0, columnspan=4)
                        wb1 = Button(warn, text="Continue anyway", command=warnContinue)
                        wb2 = Button(warn, text="Cancel and fix this", command=warnStop)
                        wb3 = Checkbutton(warn, variable=warnVar, onvalue=False, offvalue=True, text="Don't warn me anymore")
                        wb3.deselect()
                        wb1.grid(row=1, column=1)
                        wb2.grid(row=1, column=2)
                        wb3.grid(row=2, column=1, columnspan=2)
                        # warn.geometry('%dx%d+%d+%d' %(400, 300, 200, 200))
                        # print root.winfo_geometry()
                        # print warn.winfo_geometry()
                        warn.focus_set()      
                        warn.grab_set()        ## disable root window for now 
                        warn.transient(root)   ## keep on top of main window
                        root.wait_window(warn) ## wait for response

                        if (not warnReturn.get()):
                            outstem = None

                        ### simpler alternative, but doesn't allow checkbutton:
                        ## answer = tkMessageBox.askquestion("Warning", warningString, icon='warning')
                        ## print answer
                        ## if (answer == "no"):
                        ##     outstem = None 
                    
                else:
                    text1.insert(END, "Directory " + outpath + " already exists, but contains no images.\n")
                        
    text1.yview(END)
    if (outstem is not None):
        outstem = '\"' + outstem + '\"'
    return outstem


def checkVersion():
    if (len(binaryname)<1):
        text1.insert(END, 'Binary not found.\n') 
        text1.yview(END)
    else:
        analysis_command = str(binaryname + " -V")
        args = shlex.split(analysis_command)
        ## should use try/catch code here, in case binary is for wrong platform!
        version_string = subprocess.Popen(args,stdout=subprocess.PIPE).communicate()[0]
        text1.insert(END, gui_version_string + "\n")
        text1.insert(END, binaryname + ": " + version_string + "\n")
        ver_num = re.search('ersion ([\d\.]+),', version_string)
        gui_ver_num = re.search('ersion ([\d\.]+),', gui_version_string)
        ## print ver_num.group(1)
        ## print gui_ver_num.group(1)
        if ((not ver_num) or (not gui_ver_num) or (ver_num.group(1) != gui_ver_num.group(1))):
            text1.insert(END, "Warning: version number of rff-gui does not match version number for rff-bin")
        text1.yview(END)
        ## extract version number, compare to GUI version number... 


def findBinary():
    global binaryname
    myOS = sys.platform
    if (myOS == "win32"):
       binaryname1 = './rff-bin.exe'     ## slash other way depending on OS? 
       binaryname2 = 'rff-bin.exe' 
    else: 
       binaryname1 = './rff-bin'         ## slash other way depending on OS? 
       binaryname2 = 'rff-bin'   
    if (os.path.isfile(binaryname1)):
        binaryname = binaryname1
        checkVersion()
    elif (os.path.isfile(binaryname2)):
        binaryname = binaryname2
        checkVersion()
    else:
        text1.insert(END, "program not found at " + binaryname1 + " or " + binaryname2 + "\n") 
        text1.yview(END)
    


def constructCommand():

    ## print "current path: " + os.getcwd() + "\n"

    ## check that executable exists at desired location.
    ## need to cd there to be able to find xml files?
    ## should be okay if gui.py is is same dir as executable.
    ## check permission and existence for binary and image directory :
    ## os.access(path, mode)
    ## os.makedirs(path)
    
    ## global analysis_command
    global cage_num
    global video_stem

    analysis_command = None;
    
    if (len(entry1.get())<1):
        text1.insert(END, 'Please select a video first.\n') ## pop-up instead?
        text1.yview(END)
    elif (len(binaryname)<1):
        text1.insert(END, 'Binary not found.\n') 
        text1.yview(END)
    else:        
        outstem = entry2b.get() + '/' + entry2.get() ## add seperator only if missing
        outstem = outstem.replace('//','/')

        print outstem
        outstem = outstem.replace('<video_name>',video_stem)
        print outstem

        # print "old outpath zz" + outstem + "zz\n" 

        if (cage_num == 1):
            outstem = outstem.replace('<mouse_ID>',entry2L.get().strip())
            outstem = outstem.replace('<side>','left')
            outstem = checkOutstem(outstem)
            if (outstem is None):
                return None
        elif (cage_num == 2):
            outstem = outstem.replace('<mouse_ID>',entry2R.get().strip())
            outstem = outstem.replace('<side>','right')
            outstem = checkOutstem(outstem)
            if (outstem is None):
                return None
        else: ## queue both mode --- pre-validate both output paths
            global outstemLeft
            global outstemRight
            outstemLeft = outstem.replace('<mouse_ID>',entry2L.get().strip())
            outstemLeft = outstemLeft.replace('<side>','left')
            outstemLeft = checkOutstem(outstemLeft)
            if (outstemLeft is None):
                return None
            outstemRight = outstem.replace('<mouse_ID>',entry2R.get().strip())
            outstemRight = outstemRight.replace('<side>','right')
            outstemRight = checkOutstem(outstemRight)
            if (outstemRight is None):
                return None
            outstem = outstemLeft

        speedflag = '-' + var1.get()
        allflag  = '-' + var3.get()
        cropflag = '-' + var4a.get() + var4b.get() + var4c.get()
        cropflag += var5a.get() + var5b.get() + var5c.get() + var7c.get() + var7b.get()
        ## include var8 for HD-specific?

        # blank out dummy offvalues that consist just of m and _ 
        cropflag = cropflag.replace('m','')
        cropflag = cropflag.replace('_','')
        
        if (len(cropflag)==1):
            cropflag=''
      
        interlaceflag = '-' + var6.get()
        zoneflag = '' 
        if (var2.get() == 'z'):
            myzone = entry10.get()
            myzone = myzone.replace(' ','')
            zoneflag = '-z' + '' + myzone
        elif (cage_num == 1):
            zoneflag = '-zL'
        elif (cage_num == 2):
            zoneflag = '-zR'
        elif (cage_num == 3):
            zoneflag = '-zB'    

        video_name = entry1.get()
        ## remove flanking whitespace? reverse slashes for some OSes? enclose in double quotes? Only if has whitespace?
        #if (video_name.find(' ') > -1 | video_name.find('&') > -1):
        video_name = '\"' + video_name + '\"'
        inflag = '-i ' + video_name
        outflag = '-o ' + outstem
        burstflag = '-b' + entry3.get()
        gapflag = '-g' + entry4.get()
        kflag = '-' + entryk.get().replace(' = ','')

        if ((var7a.get()!='c') or ( entryAR.get()=="Default")):
           arflag = ''
        else:
           arflag = '-r'  +entryAR.get() + ' ';

        if (var2.get()=='r'):
            modevar.set("2") ## use default zones, no adustments
            
        modeflag = '-m' + modevar.get()      
        analysis_command = binaryname+' '+inflag+' '+outflag+' '+burstflag+' '+gapflag+' '+speedflag
        analysis_command += ' '+allflag+' '+interlaceflag+' '+cropflag+' '+kflag +' '+modeflag + ' ' + arflag + '' + zoneflag
        return analysis_command


def adjustZoneThenQueueLeft():
    global cage_num
    cage_num = 1
    adjustZoneThenQueue()

def adjustZoneThenQueueRight():
    global cage_num
    cage_num = 2
    adjustZoneThenQueue()

def adjustZoneThenQueueBoth2():
    adjustZoneThenQueueLeft()
    ## check if right zone already hitchhiked?
    adjustZoneThenQueueRight()

def adjustZoneThenQueueBoth():
    global cage_num
    cage_num = 3
    adjustZoneThenQueue()
       
def adjustZoneThenQueue():
    if (var2.get()=='r'):
        analysis_command = constructCommand()
        if (analysis_command is None):
            text1.insert(END, "Check configuration options.\n")
        else:
            text1.insert(END, "Adding job to queue with default zone.\n") 
            text1.insert(END, "Change 'zone to monitor' setting if you want to adjust zone first.\n")
            oc = checkQueuePaths(analysis_command)
            queue_list.append({'analysis_command':analysis_command, 'status':'pending', 'oc':oc})
            updateQueueColors()
        text1.yview(END)
        return    
    modevar.set("1")
    analysis_command = constructCommand()
    if (analysis_command is None):
        text1.insert(END, "Check configuration options.\n")
    else:
        text1.insert(END, "--------------------------------------------------------------------------\n") 
        text1.insert(END, analysis_command + "\n--------------------------------------------------------------------------\n") 
        text1.insert(END, "Adjust zone to monitor using keyboard or mouse (see help), then press c to queue job.\n")
        text1.insert(END, "To quit the analysis, click on the video window then press q.\n")
        text1.yview(END)
        root.update_idletasks()
        # root.after(1000, checkQueue)
        analysis_command = str(analysis_command)
        args = shlex.split(analysis_command)
        p = subprocess.Popen(args,stdout=subprocess.PIPE).communicate()[0] 
        save_next = 0
        new_analysis_command = None;
        for line in string.split(p, "\n"):
            if (save_next == 1):
                new_analysis_command = line.strip() # chomp? double-quote? just update zone and call construct Command?
                save_next = 0    
            if line.startswith("Command for non-interactive analysis"):
                save_next = 1
        if (new_analysis_command is not None):  ## use stronger check here?
            if (cage_num == 3):
                lastz = new_analysis_command.rfind('z')
                lastcolon = new_analysis_command.rfind(':')
                ## print new_analysis_command
                ## print lastz
                ## print lastcolon
                if ((lastz > 0) and (lastcolon > lastz)):
                    left_command = new_analysis_command[0:(lastz+1)] 
                    left_command += new_analysis_command[(lastcolon+1):len(new_analysis_command)] 
                    right_command = new_analysis_command[0:lastcolon]
                    ## need to change -o flag to outstemRight
                    ## could tokenize, or try to match outstemLeft
                    global outstemLeft
                    global outstemRight
                    ## print outstemLeft
                    ## print outstemRight
                    ## worked for test cases -- potential problems with escape chars?
                    ## right_command = right_command.replace(outstemLeft,outstemRight)
                    oflagstart = right_command.find(' -o')
                    oflagend = right_command.find(' ',oflagstart+4)
                    ## print oflagstart
                    ## print oflagend
                    ## print right_command
                    right_command = right_command[0:(oflagstart+4)] + outstemRight + right_command[oflagend:len(right_command)]
                    ## print right_command
                    oc = checkQueuePaths(left_command)
                    queue_list.append({'analysis_command':left_command, 'status':'pending', 'oc':oc, 'pid':-1})
                    updateQueueColors()
                    oc = checkQueuePaths(right_command)
                    queue_list.append({'analysis_command':right_command, 'status':'pending', 'oc':oc, 'pid':-1})
                    updateQueueColors()
                    ## text1.insert(END, p + "\n") ## move cursor to end?
                    text1.insert(END, "Commands for non-interactive analysis added to queue:\n") 
                    text1.insert(END, left_command + "\n")
                    text1.insert(END, right_command + "\n")
                else:
                    text1.insert(END, "Couldn't extract zone parameters\n.")
            else:
                oc = checkQueuePaths(new_analysis_command)
                queue_list.append({'analysis_command':new_analysis_command, 'status':'pending', 'oc':oc, 'pid':-1})
                updateQueueColors()
                ## text1.insert(END, p + "\n") ## move cursor to end?
                text1.insert(END, "Command for non-interactive analysis added to queue:\n") 
                text1.insert(END, new_analysis_command + "\n") 
        else:
            text1.insert(END, "Couldn't extract zone parameters\n.")
    text1.yview(END)
    print queue_list


def startAnalysisLeft():
    global cage_num
    cage_num = 1
    startAnalysis()

def startAnalysisRight():
    global cage_num
    cage_num = 2
    startAnalysis() 

       
def startAnalysis():
    modevar.set("0") ## adjust zones then run
    analysis_command = constructCommand()
    global sp2
    if (analysis_command is None):
        text1.insert(END, "Check configuration options.\n")
    elif (not(isinstance(sp2, int)) and (sp2.poll() is None)):
        text1.insert(END, "Process already running --- please kill that or wait till it is over.\n")
        text1.yview(END)
    else:
        text1.insert(END, "\n--------------------------------------------------------------------------\n") 
        text1.insert(END, analysis_command + "\n--------------------------------------------------------------------------\n") 
        text1.insert(END, "Adjust zone to monitor using keyboard or mouse (see help), then press c to begin analysis.\n")
        text1.insert(END, "To quit the analysis, click on the video window then press q.\n") 
        text1.yview(END)
        analysis_command = str(analysis_command)
        args = shlex.split(analysis_command)
        # print sp2
        sp2 = subprocess.Popen(args) # ,stdout=subprocess.PIPE)
        # print sp2
        # print sp2.poll() 
        # sp2.terminate()
        # print sp2
        # print sp2.poll()


def analyzeAllInQueue():
    global max_jobs
    global stop_queue
    stop_queue = 0
    ## get fresh update on num_active here?
    if (num_active > 0):
        text1.insert(END, "Queue is already active.\n") 
        text1.yview(END)
        return
    overlap_count = checkAllQueuePaths()
    if (overlap_count == 0):
        max_jobs = int(entryMP.get())
        entryMP.config(state=DISABLED) ## can't adust during run
        analyzeNextInQueue()
    else:
        print "Edit queue to avoid repeated output paths\n"
      

## this version allows more than one job one at once
## not particularly efficient, but should be okay for modest-sized queues
def analyzeNextInQueue():
    
    global spq
    global num_active

    ## kill jobs here instead?
    if (stop_queue == 1):
        text1.insert(END, "Stopping queue. Killed jobs (shown in gray) will re-start from beginning unless deleted.\n")
        text1.yview(END)
        entryMP.config(state=NORMAL)
        num_active = 0
        return

    ## clean up completed jobs:
    for i in range(0,len(queue_list)):
        if ((queue_list[i]['status']=='running')
            and not (isinstance(spq[queue_list[i]['pid']], int))
            and not (spq[queue_list[i]['pid']].poll() is None)):
            queue_list[i]['status'] = 'completed' 
            updateQueueColors()
            root.update_idletasks()

    ## check for available job slots:
    free_jobs = [];
    active_jobs = 0;
    for j in range(0,max_jobs):
        if (not(isinstance(spq[j], int)) and (spq[j].poll() is None)):
            active_jobs +=1
        else:
            spq[j] = -1 ## not strictly necessary
            free_jobs.append(j) ## could break after first
    if (len(free_jobs) == 0):   
        root.after(1000, analyzeNextInQueue)
        num_active = active_jobs
        return

    ## launch next pending job
    for i in range(0,len(queue_list)):
       if (queue_list[i]['status']=='pending') or (queue_list[i]['status']=='killed'):
            root.after(1000, analyzeNextInQueue)
            queue_list[i]['status'] = 'running'
            queue_list[i]['pid'] = free_jobs[0];
            text1.insert(END, "\n--------------------------------------------------------------------------\n") 
            text1.insert(END, queue_list[i]['analysis_command']
                         + "\n--------------------------------------------------------------------------\n") 
            text1.insert(END, "To quit the analysis, click on the video window then press q.\n")
            text1.yview(END)
            updateQueueColors()
            root.update_idletasks()
            next_command = queue_list[i]['analysis_command']
            num_active = active_jobs + 1
            # don't show video if box is checked -- add separate flag for this?
            if (var9a.get() != 'y'): next_command = next_command.replace(' -m2',' -m3')
            args = shlex.split(str(next_command))
            spq[free_jobs[0]] = subprocess.Popen(args) # stdout=subprocess.PIPE)
            return
        
    num_active = active_jobs;
    if (active_jobs > 0):
        root.after(1000, analyzeNextInQueue)    
    else:    
        text1.insert(END, "There are no pending (yellow) jobs in queue.\n")
        text1.yview(END)
        entryMP.config(state=NORMAL) ## can't adust during run
        ## root.after() not called in this case: no active and no pending jobs
   
######################
## no loner used
def checkQueue():
    global spq
    if (isinstance(spq[0], int)):
        # print "No undone jobs in queue\n"
        pass ## do nothing
    elif (spq[0].poll() is not None):
        # print "Job completed\n"
        # p = sp.communicate()[0]
        # print p
        analyzeNextInQueue()
    else: # at least one job still running
        # print "Job still running\n"
        root.after(1000, checkQueue)

#################
           
def quit(event):
    quit_botton()

def quit_button():
    global spq
    global sp2
    num_running = 0;
    for j in range(0, max_jobs):
        if not(isinstance(spq[j], int)) and (spq[j].poll() is None):
            num_running += 1
    if not(isinstance(sp2, int)) and (sp2.poll() is None):
        num_running += 1

    if num_running > 0:
        answer = tkMessageBox.askquestion("Warning",
                                          str(num_running) + " jobs are currently running.\n End them and quit anyway?",
                                          icon='warning')
        if answer == "no":
            return
        else:
            killAllCurrent() ## is this necessary, or should we let them continue running without GUI?

    ## offer to remember settings, if they have changed?        
    root.quit() 


def apply_defaults(gui_settings):

    entry2.insert(0, gui_settings['img_name'])
    entry2b.insert(0,gui_settings['img_dir'])
    entry2L.insert(0,'left_ID')
    entry2R.insert(0,'right_ID')
    entry3.insert(0,gui_settings['interval_length'])
    entry4.insert(0,gui_settings['gap_length'])

    ## set variables instead?
    if gui_settings["crop_whole"] == 'yes':
        checkbutton4a.select()
    else:
        checkbutton4a.deselect()
    
    if gui_settings["crop_zone"] == 'yes':
        checkbutton4b.select()
    else:
        checkbutton4b.deselect()

    if gui_settings["crop_face"] == 'yes':
        checkbutton4c.select()
    else:
        checkbutton4c.deselect()

    if gui_settings["with_box"] == 'yes':
        checkbutton5a.select()
    else:
        checkbutton5a.deselect()
   
    if gui_settings["without_box"] == 'yes':
        checkbutton5b.select()
    else:
        checkbutton5b.deselect()

    if gui_settings["motion"] == 'yes':
        checkbutton5c.select()
    else:
        checkbutton5c.deselect()

    entryAR.config(state=NORMAL)
    ct = 0
    while ((ct < entryAR.size()) and (entryAR.get() != gui_settings['aspect_ratio'])):
      ct+=1
      entryAR.invoke("buttonup")
      
    ct = 0
    while ((ct < entryk.size()) and (entryk.get() != gui_settings['k'])):
        ct+=1
        entryk.invoke("buttonup")

    entryMP.config(state=NORMAL)
    ct = 0
    while ((ct < entryMP.size()) and (entryMP.get() != gui_settings['max_jobs'])):
        ct+=1
        entryMP.invoke("buttonup")
    
    if gui_settings["force_aspect_ratio"] == 'yes':
        checkbutton7a.select()
    else:
        checkbutton7a.deselect()
        entryAR.config(state=DISABLED)
    
    if gui_settings["half_size"] == 'yes':
        checkbutton7b.select()
    else:
        checkbutton7b.deselect()

    if gui_settings["skip_second"] == 'yes':
        checkbutton7c.select()
    else:
        checkbutton7c.deselect()
        
    if gui_settings["show_video"] == 'yes':
        checkbutton9a.select()
    else:
        checkbutton9a.deselect()


    if gui_settings['speed'] == 'auto':
        radiobutton1a.select()
    elif gui_settings['speed'] == 'slow':
        radiobutton1b.select()
    elif gui_settings['speed'] == 'fast':
        radiobutton1c.select()


    if gui_settings['zone'] == 'manual':
        radiobutton2a.select()
        entry10.config(state=DISABLED)
    elif gui_settings['zone'] == 'default':
        radiobutton2b.select()
        entry10.config(state=DISABLED)
    elif gui_settings['zone'] == 'specify':
        radiobutton2c.select()


    if gui_settings['to_save'] == 'best':
        radiobutton3a.select()
    elif gui_settings['to_save'] == 'first':
        radiobutton3b.select()
    elif gui_settings['to_save'] == 'all':
        radiobutton3c.select()


    if gui_settings['deinterlace'] == 'none':
        radiobutton6a.select()
    elif gui_settings['deinterlace'] == 'linear':
        radiobutton6b.select()
    elif gui_settings['deinterlace'] == 'bob':
        radiobutton6c.select()

  
### assembles current gui settings into dictionary
def grab_settings():
   
    current_settings = {'img_dir': entry2b.get(),
                        'img_name': entry2.get(),
                        'interval_length': entry3.get(),
                        'gap_length': entry4.get(),
                        'speed': 'auto',
                        'deinterlace': 'linear', ## use variable values instead?
                        'zone': 'manual',
                        'crop_whole': 'no',
                        'crop_zone': 'no',
                        'crop_face': 'no',
                        'with_box': 'no',
                        'without_box': 'yes',
                        'motion': 'yes',
                        'force_aspect_ratio': 'no',
                        'aspect_ratio': entryAR.get(),
                        'skip_second':'no',
                        'half_size':'no',
                        'to_save':'best',
                        'k': entryk.get(),
                        'max_jobs': entryMP.get(),
                        'show_video':'no'
                        }
   
    if (var4a.get()=='w'): current_settings['crop_whole']='yes'
    if (var4b.get()=='j'): current_settings['crop_zone']='yes'
    if (var4c.get()=='f'): current_settings['crop_face']='yes'
   
    if (var5a.get()=='a'): current_settings['with_box']='yes'
    if (var5b.get()=='n'): current_settings['without_box']='yes'
    if (var5c.get()=='x'): current_settings['motion']='yes'
   
    if (var7a.get()=='c'): current_settings['force_aspect_ratio']='yes'
    if (var7b.get()=='h'): current_settings['half_size']='yes'
    if (var7c.get()=='e'): current_settings['skip_second']='yes'

    if (var9a.get()=='y'): current_settings['show_video']='yes'
    
    if (var1.get()=='s2'): current_settings['speed']='auto'
    if (var1.get()=='s1'): current_settings['speed']='slow'
    if (var1.get()=='s0'): current_settings['speed']='fast'
   
    if (var2.get()=='l'): current_settings['zone']='manual'
    if (var2.get()=='r'): current_settings['zone']='default'
    if (var2.get()=='z'): current_settings['zone']='specify'
   
    if (var3.get()=='p0'): current_settings['to_save']='best'
    if (var3.get()=='p1'): current_settings['to_save']='first'
    if (var3.get()=='p2'): current_settings['to_save']='all'
   
    if (var6.get()=='d0'): current_settings['deinterlace']='none'
    if (var6.get()=='d2'): current_settings['deinterlace']='linear'
    if (var6.get()=='d1'): current_settings['deinterlace']='bob'

    return current_settings



def save_settings():
    ## save settings to .gui_properties file which gets loaded at start
    ## include queue, video name, and mouse IDs, or just generic options?
    ## include history of templates?
    ## allow multiple files to restore from
    ## save "ask me again" settings from dialogs?
    pkl_file = open('gui_settings.db', 'wb')
    pickle.dump(grab_settings(), pkl_file)
    pkl_file.close()
    text1.insert(END, "Saved current GUI settings. They will be used as default settings next time.\n")
    text1.yview(END)

def clear_saved_settings():
    if (os.path.isfile('gui_settings.db')):
        os.remove('gui_settings.db')
        text1.insert(END, "Cleared saved GUI settings.\n")
    else:
        text1.insert(END, "There are no saved GUI settings to clear.\n")

    text1.yview(END)
    # or, could overwrite with default
    # pkl_file = open('gui_settings.db', 'wb')
    # pickle.dump(default_settings, pkl_file)
    # pkl_file.close()



def help_button():
    helpString =  """    - First, select the video file to analyze.
    - Then, adjust the directory and name format for output images, if desired.
    - Images will be saved with names of the form specified, with
      expression enclosed in angled brackets <> automatically replaced.
      For example <mouse_ID> will be automatically replaced by
      the left or right mouse ID, as appropriate.
      If the video names begin '<left_ID> & <right_ID> ...',
      then the mouse IDs will be extracted automatically.
      If left_ID starts with characters but right_ID starts with digits,
      the characters from left_ID will be prepended to right_ID.
      You can adust the mouse IDs if desired.
      Also <video_name> will be replaced by the name of the video,
      <side> will be replaced by either 'left' or 'right',
      <img_option> will be replaced by the name of an image output option,
      and <frame_number> will be replaced by an hour-min-sec-frame timestamp.
      No other expressions in angled brackets should be used.
    - Intervals of consecutive frames are analyzed, followed by gaps between intervals.
    - You can save all hits (frames with faces) per interval, the first k hits per interval,
      or the 'best' k hits per interval (the ones with the least movement between frames).
    - The fast mode doesn't work for some MPEGs; auto or slow should be okay.
    - Deinterlacing can be used if there's horizontal banding in the images.
    - You can either analyze a single video at a time, or just adjust the zone and
      queue video for batch processing. 
    - You can adust the zones manualy using the keyboard of mouse, or use default zones,
      or give exact coordinates for the zones.
    - To adjust the zone with the keyboard: click the video frame to make it active,
      then press u for up, d for down, l for left, r for right, t for taller,
      s for shorter, w for wider, n for narrower.
    - When you are happy with the zone (shown in blue), press c to continue (either
      add job to queue or start analysis), or press q to quit.
    - To stop the video analysis, click on the video and press q to quit.
    - There is an option to display and analyze videos at half-size, for speed.
      In this case, the output images without boxes are saved at full-size.
    - In the queue, not-yet-run jobs are shown with yellow background, the current job
      with green background, and completed jobs with red background.
    - If the text of a job in the queue is red, the job has the same output path as a
      previously queued job, so images may be overwritten or intermingled.
    - You can either delete these jobs and re-queue them after changing the mouse ID or
      image path, or you can edit the outpath (option after -o) in the queue.
      Warning: Editing the directory names may cause things to break, but the image
      name format should be okay""" 
    
    helpwin = Toplevel()
    helpwin.title("Instructions")
    helpmsg = Message(helpwin, text=helpString, aspect=200) ## width=80 --- should be in chars, but seems to be in pixels 
    helpmsg.pack()
    helpbutton = Button(helpwin, text="OK", command=helpwin.destroy)
    helpbutton.pack()

    # tkMessageBox.showinfo("Instructions", helpString)


def about_button():
    aboutString =  "    "+ gui_version_string + """
    Copyright 2011 Boston Biomedical Research Institute (BBRI)
    Author: Oliver D. King, king@bbri.org

    THE SOFTWARE IS LICENSED ON AN 'AS IS' BASIS AND BBRI MAKES NO
    REPRESENTATIONS OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY,
    OR OTHERWISE, WITH RESPECT TO THE SOFTWARE, OR OTHER ACCOMPANYING
    MATERIAL OR SERVICE, IF ANY, AND BBRI SPECIFICALLY DISCLAIMS ALL
    EXPRESS AND IMPLIED WARRANTIES, INCLUDING WITHOUT LIMITATION, ALL
    IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
    PURPOSE, AND NON-INFRINGEMENT, OR WARRANTIES ARISING BY STATUTE
    OR OTHERWISE IN LAW OR FROM A COURSE OF DEALING OR USE OF TRADE.
    BBRI DOES NOT WARRANT THAT THE OPERATION OR OTHER USE OF THE
    SOFTWARE WILL BE UNINTERRUPTED OR ERROR FREE OR WILL NOT CAUSE
    DAMAGE OR DISRUPTION TO END USER'S DATA, COMPUTERS OR NETWORKS.

    See LICENSE.TXT for complete license information. 
    """ 
    
    aboutwin = Toplevel()
    aboutwin.title("Instructions")
    aboutmsg = Message(aboutwin, text=aboutString, aspect=200) ## width=80 --- should be in chars, but seems to be in pixels 
    aboutmsg.pack()
    aboutbutton = Button(aboutwin, text="OK", command=aboutwin.destroy)
    aboutbutton.pack()



def activate_zone():
    entry10.config(state=NORMAL)    

def deactivate_zone():
    entry10.config(state=DISABLED)

def activate_entryk():
    entryk.config(state=NORMAL)    

def deactivate_entryk():
    entryk.config(state=DISABLED)      

def toggle_entryAR():
    if (var7a.get()=='c'):
       entryAR.config(state=NORMAL)
    else:
       entryAR.config(state=DISABLED)
        
def deleteSelected():
    indices = queuebox.curselection()
    for i in indices[::-1]: ## in reverse, so indices dont shift
        if queue_list[int(i)]['status']=='running':
            text1.insert(END, "Need to kill running jobs before deleting them.\n")
            text1.yview(END)
        else:
            del queue_list[int(i)]
    updateQueueColors()

def editQueue(index, new_command):
    editwin.destroy()
    oldfg = queuebox.itemcget(index,'fg')
    oldbg = queuebox.itemcget(index,'bg')
    ## indices may be strings, so index+ 1 doesn't work! delete first, then add new
    queue_list[int(index)]['analysis_command'] = new_command ## change status if text changed?
    updateQueueColors()
    
## Launch pop-up window to let user edit queued command.
## May need to create output directories!!! Not yet tested fully!
## Bind to double-click of queuebox?
def editSelected():
    global editwin
    indices = queuebox.curselection()
    if ((indices is None) or (len(indices) == 0)):
       return
    i = indices[0]
    editwin = Toplevel() 
    editwin.title("Edit output path")
    editentry = Entry(editwin, width=100)
    editentry.configure(font = "Courier 8")
    editentry.insert(END, queuebox.get(i))
    editentry.configure(bg=queuebox.itemcget(i,'bg'), fg=queuebox.itemcget(i,'fg'))
    editentry.grid(row=0, column=0, columnspan=2)
    editbutton1 = Button(editwin, text="Accept", command=lambda: editQueue(i, editentry.get()))
    editbutton2 = Button(editwin, text="Cancel", command=editwin.destroy)
    editbutton1.grid(row=1, column=0, sticky=E)
    editbutton2.grid(row=1, column=1, sticky=W)
    

def clearCompleted():
    indices = range(0,len(queue_list));
    for i in indices[::-1]: ## in reverse, so indices dont shift
        if (queue_list[i]['status']=='completed'): ## also clear killed jobs?
            del queue_list[i]
    updateQueueColors()

def killAllCurrent():
    global sp2
    global spq
    global stop_queue
    
    if isinstance(sp2, int):
        text1.insert(END, "No stand-alone process to kill\n")
    elif sp2.poll() is None:
        sp2.terminate() ## send signal equiv to keypress Q instead of ctrl-C?
    sp2 = -1 ## make an assertion? try/catch?

    indices = range(0,len(queue_list));
    for j in range(0, max_jobs):
        if isinstance(spq[j], int):
            text1.insert(END, "Queue process " + str(j+1) + " is not running.\n")
            text1.yview(END)
        elif spq[j].poll() is None:
            text1.insert(END, "Killing process " + str(j+1) + " from queue.\n")
            text1.yview(END)
            spq[j].terminate() ## send signal equiv to keypress Q?        
            for i in indices: 
                if (queue_list[i]['status']=='running') and (queue_list[i]['pid']==j):
                    queue_list[i]['status']='killed'
                    ## break
            updateQueueColors()       
        spq[j] = -1 ## make an assertion? try/catch?

    stop_queue = 1


def killSelected():
    global spq
    indices = queuebox.curselection()
    if ((indices is None) or (len(indices) == 0)):
       return
    i = int(indices[0])
    j = queue_list[i]['pid']
    print "selected",i,j
    if (j < 0) or (queue_list[i]['status']!='running') or (isinstance(spq[j], int)):
        text1.insert(END, "Selected job is not currently running.\n")
        text1.yview(END)
    elif (spq[j].poll() is None):
        text1.insert(END, "Killing selected job.\n")
        text1.yview(END)
        spq[j].terminate() ## send signal equiv to keypress Q?
        queue_list[i]['status']='killed'
        updateQueueColors()       



##### ROOT WINDOW #####

# Menu (in menu bar)
menubar = Menu(root)

file_menu = Menu(menubar, tearoff=0)
file_menu.add_command(label='Quit (ctrl-q)', command=quit_button)
file_menu.add_command(label='About', command=about_button)
file_menu.add_command(label='Help', command=help_button)
file_menu.add_command(label='Save Settings', command=save_settings)
file_menu.add_command(label='Clear Saved Settings', command=clear_saved_settings) 
# file_menu.add_separator() 
menubar.add_cascade(label='File', menu=file_menu) 
root.config(menu=menubar) # ??

label0 = Label(root,text="Configure options below, then analyze video or add video to queue", fg="black", bg="white")
label1 = Label(root,text="video to analyze", fg="red", bg="white")
#label3 = Label(root,text="interval length (in seconds)", fg="red", bg="white")
#label4 = Label(root,text=" gap between intervals (in seconds)", fg="red", bg="white")
label4x = Label(root,text=" gap length (in seconds)", fg="white", bg="white") ## for LR symmetry
label2 = Label(root,text="format for image names", fg="red", bg="white")
label2b = Label(root,text="image subdirectory", fg="red", bg="white")
label6 = Label(root,text="     ", fg="red", bg="white")

#label5 = Label(root, text="Save one frame per this many seconds", fg="red",bg="white")
#entry5 = Entry(root, width=8)

label7 = Label(root,text="  frames saved per interval", fg="red", bg="white")
label8 = Label(root,text="speed", fg="red", bg="white")
label13 = Label(root,text="deinterlacing", fg="red", bg="white")
label9 = Label(root,text="zone to monitor", fg="red", bg="white")
label10 = Label(root,text="zone coords", fg="red", bg="white")
label12 = Label(root,text="image options", fg="red", bg="white")
label11 = Label(root,text="cropping option", fg="red", bg="white")
label7a = Label(root,text="other options", fg="red", bg="white")
label8a = Label(root,text="overrides for HD videos", fg="red", bg="white")

# labelk = Label(root,text="k =", fg="red", bg="white")

button2L = Button(root, text="Analyze left cage", bg="green", command=startAnalysisLeft)
button2R = Button(root, text="Analyze right cage", bg="green", command=startAnalysisRight)
button3L = Button(root, text="Queue left cage", bg="green", command=adjustZoneThenQueueLeft)
button3R = Button(root, text="Queue right cage", bg="green", command=adjustZoneThenQueueRight)
button3LR = Button(root, text="Queue both cages", bg="green", command=adjustZoneThenQueueBoth)
button4 = Button(root, text="Analyze all pending jobs", bg="green",command=analyzeAllInQueue)
button5 = Button(root, text="Delete selected jobs", bg="green",command=deleteSelected)
button6 = Button(root, text="Clear completed jobs", bg="green",command=clearCompleted)
button7 = Button(root, text="Kill active jobs and stop", bg="green",command=killAllCurrent)
# button7 = Button(root, text="Kill selected", bg="green",command=killSelected)
button8 = Button(root, text="Edit selected job", bg="green",command=editSelected)

entry1 = Entry(root)
entry2 = Entry(root)
entry2b = Entry(root)
## grey this out, unless file loaded?
## entry2.config(state=DISABLED)
entry2L = Entry(root, width=12)
entry2R = Entry(root, width=12)
entry3 = Entry(root, width=6)
entry4 = Entry(root, width=6)
entry6 = Entry(root)
entry7 = Entry(root)
entry8 = Entry(root)
entry9 = Entry(root)
entry10 = Entry(root, width=16)


entryk = Spinbox(root, values = ["k = 1", "k = 2", "k = 3", "k = 4", "k = 5",
                                 "k = 6", "k = 7", "k = 8", "k = 9", "k = 10"], width=6)


entryAR = Spinbox(root, values = ["Default", "1:1", "4:3", "16:9", "16:10",
                                  "2.35:1"], width=6)

entryMP = Spinbox(root, values = ["1", "2", "3", "4", "5", "6", "7", "8"], width=6)


button1 = Button(root, text="Choose video to analyze", command=chooseVideo)

queuebox = Listbox(root, selectmode=EXTENDED)
queuebox.configure(bd=4)
queuebox.configure(font = "Courier 8")

queue_list = [] ## list of dictionaries with attributes of queued jobs

# button2 = Button(root)

# checkbutton1 = Checkbutton(root)
# checkbutton2 = Checkbutton(root)
# checkbutton3 = Checkbutton(root)

text1 = Text(root, bg="gray", bd=5, padx=5, pady=5, height=8)
text1.configure(font = "Courier 8")

var1 = StringVar() # 0
var2 = StringVar() # 3
var3 = StringVar() # 6
var4 = StringVar() # 9
var5 = StringVar() # 12
var6 = StringVar() # 12

var4a = StringVar() # 9
var4b = StringVar() # 9
var4c = StringVar() # 9

var5a = StringVar() # 9
var5b = StringVar() # 9
var5c = StringVar() # 9

var7a = StringVar()
var7b = StringVar() 
var7c = StringVar() 

var8a = StringVar()
var8b = StringVar() 
var8c = StringVar()

var9a = StringVar()
var9b = StringVar() 
var9c = StringVar()

modevar = StringVar()

###############
radiobutton1a = Radiobutton(root, variable=var1,value='s2',text="auto",bg="white") 
radiobutton1b = Radiobutton(root, variable=var1,value='s1',text="slow",bg="white")
radiobutton1c = Radiobutton(root, variable=var1,value='s0',text="fast",bg="white")

## l and r here no longer correspond to right and left! right and left are determined by which start button is pressed  
radiobutton2a = Radiobutton(root, variable=var2,value='l',text="adjust manually",bg="white", command=deactivate_zone)
radiobutton2b = Radiobutton(root, variable=var2,value='r',text="use default",bg="white", command=deactivate_zone)
radiobutton2c = Radiobutton(root, variable=var2,value='z',text="specify coordinates:",bg="white", command=activate_zone)

radiobutton3a = Radiobutton(root, variable=var3,value='p0',text="best k hits",bg="white", command=activate_entryk) # ugly -- labda expression?
radiobutton3b = Radiobutton(root, variable=var3,value='p1',text="first k hits",bg="white", command=activate_entryk)
radiobutton3c = Radiobutton(root, variable=var3,value='p2',text="all hits",bg="white", command=deactivate_entryk)
# radiobutton3d = Radiobutton(root, variable=var3,value='p3',text="all-frames",bg="white")

radiobutton6a = Radiobutton(root, variable=var6,value='d0',text="none",bg="white")
radiobutton6b = Radiobutton(root, variable=var6,value='d2',text="linear",bg="white")
radiobutton6c = Radiobutton(root, variable=var6,value='d1',text="bob",bg="white")

## if offvalues are repeated, buttons seems to get slaved to one another! a bug?
checkbutton4a = Checkbutton(root, variable=var4a,onvalue='w',offvalue="m_",text="whole frame",bg="white")
checkbutton4b = Checkbutton(root, variable=var4b,onvalue='j',offvalue="mm_",text="just zone",bg="white")
checkbutton4c = Checkbutton(root, variable=var4c,onvalue='f',offvalue="mmm_",text="just face",bg="white")

checkbutton5a = Checkbutton(root, variable=var5a,onvalue='a',offvalue="_m",text="with boxes",bg="white")
checkbutton5b = Checkbutton(root, variable=var5b,onvalue='n',offvalue="_mm",text="without boxes",bg="white")
checkbutton5c = Checkbutton(root, variable=var5c,onvalue='x',offvalue="_mmm",text="motion view",bg="white")

checkbutton7a = Checkbutton(root, variable=var7a,onvalue='c',offvalue="_m_",text="force aspect-ratio:",bg="white", command=toggle_entryAR)
checkbutton7b = Checkbutton(root, variable=var7b,onvalue='h',offvalue="_mm_",text="analyze at half-size",bg="white")
checkbutton7c = Checkbutton(root, variable=var7c,onvalue='e',offvalue="_mmm_",text="skip first second",bg="white")

checkbutton8a = Checkbutton(root, variable=var8a,onvalue='C',offvalue="m_m_",text="force 16:9 aspect",bg="white")
checkbutton8b = Checkbutton(root, variable=var8b,onvalue='H',offvalue="m_mm_",text="analyze at half-size",bg="white")
checkbutton8c = Checkbutton(root, variable=var8c,onvalue='E',offvalue="m_mmm_",text="skip first second",bg="white")

checkbutton9a = Checkbutton(root, variable=var9a,onvalue='y',offvalue="_m_m_",text="display video",bg="white")

# label0.grid(sticky=EW, columnspan=5)
# label1.grid(sticky=E)
label2b.grid(sticky=E, row=1, column=0)
label2.grid(sticky=E, row=2,  column=0)

Label(root,text="   interval length (in seconds)", fg="red", bg="white").grid(row=4, sticky=E, column=0)
Label(root,text=" gap length (in seconds)", fg="red", bg="white").grid(row=4, sticky=E, column=2)
Label(root,text="mouse_ID for left cage", fg="red", bg="white").grid(row=3, sticky=E, column=0)
Label(root,text="mouse_ID for right cage", fg="red", bg="white").grid(row=3, sticky=E, column=2)

label4x.grid(row=4,column=4,sticky=W)

#label6.grid(row=5,sticky=EW, columnspan=5)
label7.grid(row=6,sticky=E)
label8.grid(row=7,sticky=E)
label9.grid(row=9,sticky=E)
# label10.grid(row=11,sticky=E)
label11.grid(row=11,sticky=E)
label12.grid(row=12,sticky=E)
label13.grid(row=8,sticky=E)

entry1.grid(row=0, column=1, columnspan=4,sticky=EW)

entry2b.grid(row=1, column=1, columnspan=4,sticky=EW)
entry2.grid(row=2, column=1, columnspan=4,sticky=EW)


entry3.grid(row=4, column=1, columnspan=1,sticky=W)
entry4.grid(row=4, column=3, columnspan=1,sticky=W)
#label5.grid(row=5, column=0, columnspan=2, sticky=E)
#entry5.grid(row=5, column=2, columnspan=1,sticky=W)

entry10.grid(row=9, column=4,columnspan=1,sticky=W)
entry10.insert(0,'10,10,200,200')
## grey this out, unless radiobutton checked.

radiobutton2a.grid(row=9, column=1, sticky=W)
radiobutton2b.grid(row=9, column=2, sticky=W)
radiobutton2c.grid(row=9, column=3, sticky=W)

radiobutton1a.grid(row=7, column=1, sticky=W)
radiobutton1b.grid(row=7, column=2, sticky=W)
radiobutton1c.grid(row=7, column=3, sticky=W)

#radiobutton2a.grid(row=9, column=1, sticky=W)
#radiobutton2b.grid(row=9, column=2, sticky=W)
entry2L.grid(row=3,column=1,sticky=EW)
entry2R.grid(row=3,column=3,sticky=EW)


radiobutton3a.grid(row=6, column=3, sticky=W)
radiobutton3b.grid(row=6, column=2, sticky=W)
radiobutton3c.grid(row=6, column=1, sticky=W)
entryk.grid(row=6, column=4, sticky=W)
# labelkgrid(row=5, column=4, sticky=W)

checkbutton4a.grid(row=11, column=1, sticky=W)
checkbutton4b.grid(row=11, column=2, sticky=W)
checkbutton4c.grid(row=11, column=3, sticky=W)

checkbutton5a.grid(row=12, column=1, sticky=W)
checkbutton5b.grid(row=12, column=2, sticky=W)
checkbutton5c.grid(row=12, column=3, sticky=W)

radiobutton6a.grid(row=8, column=1, sticky=W)
radiobutton6b.grid(row=8, column=2, sticky=W)
radiobutton6c.grid(row=8, column=3, sticky=W)

label7a.grid(row=13,sticky=E)
checkbutton7a.grid(row=13, column=3, sticky=W)
checkbutton7b.grid(row=13, column=2, sticky=W)
checkbutton7c.grid(row=13, column=1, sticky=W)
# Label(root,text="  force aspect ratio:", fg="black", bg="white").grid(row=13, column=3, sticky=E)
entryAR.grid(row=13, column=4, sticky=W)

# label8a.grid(row=14,sticky=E)
# checkbutton8a.grid(row=14, column=1, sticky=W)
# checkbutton8b.grid(row=14, column=2, sticky=W)
# checkbutton8c.grid(row=14, column=3, sticky=W)

#image.grid(row=0, column=2, columnspan=2, rowspan=2,  sticky=W+E+N+S, padx=5, pady=5)
# Label(root,text="   ", fg="red", bg="white").grid(row=14)
button1.grid(row=0,  column=0, sticky=EW)

button2L.grid(row=15, column=0, columnspan=1, sticky=EW)
button2R.grid(row=15, column=1, columnspan=1, sticky=EW)
button3L.grid(row=15, column=2, columnspan=1, sticky=EW)
button3R.grid(row=15, column=3, columnspan=1, sticky=EW)
button3LR.grid(row=15, column=4, columnspan=1, sticky=EW)

# button4.grid(row=15, column=3, columnspan=1, sticky=EW)
# button5.grid(row=15, column=4, columnspan=1, sticky=EW)

# scrollbar1 = Scrollbar(text1)
#scrollbar1.pack(side=RIGHT, fill=BOTH)
#text1.config(yscrollcommand = scrollbar1.set, height=20)
#scrollbar1.config(command = text1.yview)

text1.grid(row=16, column=0, columnspan=5, sticky=NSEW)

   
## delete selected
## analyze selected
## analyze all
Label(root, text="queue options",fg="red",bg="white").grid(row=17, column=0, sticky=E)
checkbutton9a.grid(row=17, column=1, sticky=W)
Label(root, text="max number of simultaneous processes: ",fg="black",bg="white").grid(row=17, column=2, columnspan=2, sticky=E)
entryMP.grid(row=17, column=4, sticky=W)
queuebox.grid(row=18, column=1, columnspan=4, rowspan=5,sticky=NSEW)
button4.grid(row=18, column=0, sticky=EW)
button5.grid(row=19, column=0, sticky=EW)
button6.grid(row=20, column=0, sticky=EW)
button7.grid(row=21, column=0, sticky=EW)
button8.grid(row=22, column=0, sticky=N+E+W)

for k in range(0,5):
   root.columnconfigure(k, weight=8)
for k in range(0,22):
   root.rowconfigure(k, weight=0)
root.rowconfigure(22, weight=1)

# Keyboard Commands
root.bind("<Control-q>", quit) # Quits window by pressing control-q
## bind delete key to queue if it has focus?
## allow re-sorting of queuebox 


default_settings = {'img_dir': 'images/<mouse_ID>/<img_option>',
                    'img_name': '<img_option>_<video_name>_<frame_number>.png',
                    'interval_length': '40',
                    'gap_length': '20',
                    'speed': 'auto',
                    'deinterlace': 'linear',
                    'zone': 'manual',
                    'crop_whole': 'yes',
                    'crop_zone': 'yes',
                    'crop_face': 'yes',
                    'with_box': 'yes',
                    'without_box': 'yes',
                    'motion': 'yes',
                    'force_aspect_ratio':'no',
                    'aspect_ratio':'16:9',
                    'skip_second':'no',
                    'half_size':'no',
                    'to_save':'best',
                    'k':'k = 3',
                    'max_jobs':'1',
                    'show_video':'yes'
                    }

#########################
###### load saved settings
#########################

gui_settings = default_settings

try:
    pkl_file = open('gui_settings.db', 'rb')
    saved_settings = pickle.load(pkl_file)
    ##  merge with defaults, in case some fields are not defined in pickle
    gui_settings.update(saved_settings)
    print "loading saved GUI settings"
    pkl_file.close()
except:
    print "no saved GUI settings to load"

######################

apply_defaults(gui_settings)
findBinary()

# root.after(1000,checkQueue)
root.mainloop() # run until quit

##### END #####


