#!/usr/bin/env python2
# -*- coding: utf-8 -*-
"""
Created on Wed Mar 22 20:19:54 2017

@author: atabak
"""

import os
import cv2
import numpy as np
#from random import shuffle


base = os.path.dirname(os.path.realpath(__file__))
objs = os.listdir(os.path.join(base,'new_data'))
imgs = {'cylinder':list(), 'other':list()}
for obj in objs:
  idxs_path = os.path.join(base, 'new_data', obj)
  idxs = os.listdir(idxs_path)
  for idx in idxs:
    imgs_path = os.path.join(idxs_path, idx)
#  imgs_path = os.path.join(os.path.join(base,'cylinder_dataset'),obj)
    for img in os.listdir(imgs_path):
      if obj=='cylinder':
        imgs['cylinder'].append(cv2.imread(os.path.join(imgs_path, img)))
      else:
        imgs['other'].append(cv2.imread(os.path.join(imgs_path, img)))
      
      
def write_imgs(label='cylinder'):
  counter_test = 0
  counter_train = 0
  
  def select_fate(img, counter_train, counter_test):
    rnd = np.random.rand()
    if rnd < 0.1:
      cv2.imwrite(os.path.join(base, 'test', label+'.'+str(counter_test)+'.jpg'), img)
      counter_test+=1
    else:
      cv2.imwrite(os.path.join(base, 'train', label+'.'+str(counter_train)+'.jpg'), img)
      counter_train+=1
    return counter_train, counter_test
  imgs_list=imgs[label]
  for img in imgs_list:
    counter_train, counter_test = select_fate(img, counter_train, counter_test)
    img  = cv2.flip(img,1)
    counter_train, counter_test = select_fate(img, counter_train, counter_test)
    img = cv2.flip(img,0)
    counter_train, counter_test = select_fate(img, counter_train, counter_test)
    img = cv2.flip(img,1)
    counter_train, counter_test = select_fate(img, counter_train, counter_test)
    
    
#def write_imgs(label='cylinder'):
#  counter_train = 0
##    rnd = np.random.rand()
##    if rnd < 0.2:
##      cv2.imwrite(os.path.join(base, 'test', label+'.'+str(counter_test)+'.jpg'), img)
##      counter_test+=1
##    else:
##    cv2.imwrite(os.path.join(base, 'train', label+'.'+str(counter_train)+'.jpg'), img)
#  imgs_list=imgs[label]
#  temp_list = list()
#  for img in imgs_list:
#    temp_list.append(img)
#    img  = cv2.flip(img,1)
#    temp_list.append(img)
#    img = cv2.flip(img,0)
#    temp_list.append(img)
#    img = cv2.flip(img,1)
#    temp_list.append(img)
#  imgs_list = None
#  shuffle(temp_list)
#  for img in temp_list:
#    cv2.imwrite(os.path.join(base, 'train', label+'.'+str(counter_train)+'.jpg'), img)
#    counter_train+=1
#    
write_imgs()
write_imgs('other')
      
      
