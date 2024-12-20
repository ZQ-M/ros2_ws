from setuptools import find_packages
from setuptools import setup

setup(
    name='vr_interfaces',
    version='0.0.0',
    packages=find_packages(
        include=('vr_interfaces', 'vr_interfaces.*')),
)
