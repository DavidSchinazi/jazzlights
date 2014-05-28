import os
import sys
import glob
import logging
import subprocess

from .player import FPS

def main():
    if len(sys.argv)<3:
        print "Usage: %s <input dir> <output dir>" % sys.argv[0]
        exit(0)
    else:
        (indir,outdir) = sys.argv[1:3]	

    if not os.path.exists(outdir):
    	os.makedirs(outdir)

    with open(outdir + '/playlist.m3u', 'w') as playlist:
	    for infile in glob.glob( indir + '/*.mp4'):
	        basename = os.path.basename(infile)[:-4]
	        playlist.write( basename + '.mp3\n')
	        outpath = outdir + '/' + basename
	        if not os.path.exists(outpath):
	        	os.makedirs(outpath)

	        start_t = 30
	        duration = 60
	        width = 500
	        height = 50

	        subprocess.check_call(['ffmpeg', '-y', 
	        	'-ss', str(start_t), 
	        	'-i', infile, 
	        	'-t', str(duration),
	        	'-r', str(FPS),
	        	'-vf', 'scale=w=%d:h=-1' % width,
	        	#'-vf', 'crop=%d:%d' % (width, height), 
	        	outpath + '/frame%5d.jpg'
	        	])
	        subprocess.check_call(['ffmpeg', '-y', 
	        	'-ss', str(start_t), 
	        	'-i', infile, 
	        	'-t', str(duration),
	        	outpath + '.mp3'
	        	])
