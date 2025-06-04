# Development Log

> 🧪 **Note:** This README serves as my personal development log for the Coral Micro CatDog camera streaming app. It documents technical decisions, setbacks, and successful configurations to support my master's thesis on edge ML systems.

This project is designed to help you get familiar with the Google Coral Dev Board Micro and its built-in API, including LED control, camera streaming, and eventually ML model deployment. 

After overcoming several setup challenges, this app now successfully streams real-time camera frames from the board to a Linux machine—offering a solid foundation for further development and inference testing.

## What I've Done

First of all, I made a LED Blinking app to see whether everything is functioning correctly. I found out that using Type-C to Type-C cable works but not USB-A to Type-C for reasons I don't understand. Then, I attempted to deploy a TFLite model using ChatGPT guidance but encountered compatibility issues. Therefore, I revert back to this version where I try to stream camera onto my linux machine by referring to the examples. At least now I know I can see what the camera see and it will likely help identify whether a model is working correctly. 

## How to execute the app? 

First, have a linux machine (I have an ubuntu 22 on a think pad), a Type-C cable, and a coral micro dev board. You don't have to plug in initially. 

Now open up a terminal and clone a repository into your machine: 

```
git clone https://github.com/daniel7722/coralmicro.git
```

or you can fork it from my github account.

Then, you need to run: 

```
cd coralmicro (or whatever you named your cloned repo)
bash setup.sh
bash build.sh
```

The nitty gritty of this particular repo is that I only succeeded at setting things up with a python virtual env of version 3.8.10. Any python version higher than that would likely results in missing dependencies that are complicated to download. 

If everything works, it means that you can flash all of the apps or examples onto the board. To start any app, you need to plug in the board first. An orange LED on the Coral board should illuminate when powered successfully. run `lsusb` to see if your computer can find the device. 

Now you can flash through: 
```
python3 scripts/flashtool.py -a <app name> (or -e <example name>)
```

Then, you should see the terminal saying it's restarting the device. After seeing this message, open another terminal to access serial console

```
ls /dev/ttyACM*
screen /dev/ttyACM0
```
Depends on the output of ls, normally it would appear `ttyACM0`. If it shows `[screen is terminating...]`, wait for a few second and try `screen` command again until `screen` is displayed. Then, use the first terminal window to run the `stream.py` client script to initiate communication and trigger camera streaming.

```
python3 apps/catdog_blinker/stream.py
```
Now you should be able to see the streaming camera and Voila. 



## Streaming and Logging
### [2025-06-04]
It turns out streaming and logging at the same time is really annoying because both tasks are sharing one usb connection and will need to deal with concurrent access and resource contention. For a good month I was trying to do this and have failed and restarted multiple times. Since this is inspired by multicore_model_cascade app built in the repo. I figured it'd be easier to copy it and omit unrelated lines of code. Here's a general picture of the app. 

- Start ***watchdog*** to monitor and babysit microcontroller
- Initialise ***TPU manager*** for future TPU-based models deployment
- Create FreeRTOS tasks (MainTask, NetworkTask). 
- Suspend main thread


The threads operates as the follow: 
Main Function Thread
├── Main Task Thread
│   ├── Waits for `isSetup` flag from NetworkTask
│   ├── Powers on and enables the camera
│   ├── Captures frames continuously
│   └── Sends compressed JPEGs via NetworkTask
└── Network Task Thread
    ├── Opens socket server on port 27000
    ├── Waits for incoming client connection
    ├── Sets `isSetup = true` upon successful connection
    └── Provides `Send()` method used by both tasks
