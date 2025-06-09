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

Then, you should see the terminal saying it's restarting the deviceee. After sing this message, open another terminal to access serial console.

```
ls /dev/ttyACM*
screen /dev/ttyACM0
```
Depends on the output of ls, normally it would appear `ttyACM0`. If it shows `[screen is terminating...]`, wait for a few second and try `screen` command again until `screen` is displayed. Then, use the first terminal window to run the `stream.py` client script to initiate communication and trigger camera streaming.

```
python3 apps/catdog_blinker/stream.py
```
Now you should be able to see the streaming camera and Voila. 

By the way, if changes are made and a rebuild is neccessary: 
```
make -C build/apps/catdog_blinker/
```
This should rebuild only this part of the repo. 



## Streaming and Logging
### [2025-06-04]
It turns out streaming and logging at the same time is really annoying because both tasks are sharing one usb connection and will need to deal with concurrent access and resource contention. For a good month I was trying to do this and have failed and restarted multiple times. Since this is inspired by multicore_model_cascade app built in the repo. I figured it'd be easier to copy it and omit unrelated lines of code. Here's a general picture of the app. 

- Start ***watchdog*** to monitor and babysit microcontroller
- Initialise ***TPU manager*** for future TPU-based models deployment
- Create FreeRTOS tasks (MainTask, NetworkTask). 
- Suspend main thread


The threads operates as the follow: 
```
---------------------------------------------------------------------
|                                                                   |
|                    **Main Function Thread**                       |
|                                                                   |
---------------------------------------------------------------------
              |                                     |
              |                                     |
-------------------------------     ---------------------------------
|    **Main Task Thread**     |     |    **Network Task Thread**    |
|                             |     |                               |
|  - Waiting for network task |     |  - Open socket server and     |
|  to switch flag (isSetup)   |     |  wait for a client connection |
|  - Enable Camera, capture   |---->|  - Send() sends data to       |
|  frames and send through    |     |  client. Used by both Tasks   |
|  network task.              |     |                               |
|                             |     ---------------------------------
-------------------------------
```

## Tiny Machine Learning Model
### [2025-06-05]

Previously, I have tested mobilenetv3 fine-tuned on cats and dogs images. However, when I try to deploy on the board, it failed. Multiple reasons might have cause this. First, it could be the size of the model being too large, silently crashes the whole board. Second, the operations are not well-defined so TPU aren't really capable of interpret the model. Or, it could be that I forgot to include DATA in CMakeLists.txt. But I've got it working finally, but the command `screen` is really slow and will lose many valuable outputs. I guess it has to stay this way for a bit. I've tried deploying the model, which seems to be not crashing anything but the model was doing anything. It might be the input output sizes problems, or model itself not able to run inference. Also, we found out that the model inference is much slower than I expected. It logs score every 1 minute and that is just not enought. Through a series of printf all over the place, I found out that the inference task's queue is always full and the inference speed isn't catching up with the queuing speed. That's why I use QueuePeek to block the queue from getting enqueue while processing it and it definitely has some synchronisation improvement. 

## Machine Learning Model

Knowing the model inference is slow regardless of concurrency magic do, today I try to tackle the machine learning model itself. I start with examining the model input. We found that the input size of the camera is 324 by 324 while the model was trained on 224 by 224. Hence, we modify that during preprocessing. Also, we change it to softmax, outputing class score. Also, to better fit onto edgeTPU, a post-traininig∂ quantisation was done to make the weight unsigned interger 8 byte. After all this, we successfully create something that can be run. However as I compile the model and checked the log message, it disclose that most of the operation in mobilenetv3 isn't used by TPU and would use a lot of CPU to process the information. This is the opposite of what we want  my edgetpu compiled model has 4 out of 110 operations runs on tpu, which is unacceptable. Chat suggests to use other model with better compatible base architecture. 

Knowing the model inference is slow regardless of concurrency magic do, today I try to tackle the machine learning model itself. I start with examining the model input. We found that the input size of the camera is 324 by 324 while the model was trained on 224 by 224. Hence, we modify that during preprocessing. Also, we change it to softmax, outputing class score. Also, to better fit onto edgeTPU, a post-traininig∂ quantisation was done to make the weight unsigned interger 8 byte. After all this, we successfully create something that can be run. However as I compile the model and checked the log message, it disclose that most of the operation in mobilenetv3 isn't used by TPU and would use a lot of CPU to process the information. This is the opposite of what we want. Therefore, Chat suggested me to move to mobilenetv2 which is more compatible with edgeTPU. It actually works quite well. I've tried softmax with binary classification but it ended up with dog score higher than cat score when pointing nothing, but in general it can distinguish between the two animals, which is a significant step. Tried to add a none class for when either cats or dogs are in frame, but failed as there is no good none class example image that is general enough to be represented.

I expect to start another ml model to make this multicore. 