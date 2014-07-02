import os
from setuptools import setup, find_packages

here = os.path.abspath(os.path.dirname(__file__))

requires = [
    'dxfgrabber',
    'flask-socketio',
    'pillow',
    'python-mpd',
  ]

setup(name='dfplayer',
      version='0.1',
      description='DiscoFish Player',
      classifiers=[
        "Programming Language :: Python",
        ],
      author='Dmitry Azovtsev',
      author_email='dmitry@azovtsev.com',
      url='http://localhost/',
      packages=find_packages(),
      include_package_data=True,
      zip_safe=False,
      install_requires=requires,
      entry_points="""\
      [console_scripts]
      dfplayer = dfplayer:main
      dfprepr = dfplayer.preprocess:main
      test = dfplayer.test:main
      """,
      )
