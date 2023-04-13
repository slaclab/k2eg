from setuptools import setup, find_packages

VERSION = '0.1.0'
DESCRIPTION = 'A k2eg client'
LONG_DESCRIPTION = 'Permit to submit command and listen from result using k2eg gateway'

setup(
    name='k2egcli',
    version=VERSION,
    description=DESCRIPTION,
    long_description=LONG_DESCRIPTION,
    author="Claudio Bisegni",
    author_email="bisegni@slac.stanford.edu",
    py_modules=['k2egcli'],
    packages=find_packages(),
    install_requires=[
        'typer',
        'colorama',
        'shellingham',
        'kafka_python',
        'python_snappy',
    ],
    entry_points={
        'console_scripts': [
            'k2egcli = k2egcli.__main__:main',
        ],
    },
    classifiers= [
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        'License :: OSI Approved :: MIT License',
        "Programming Language :: Python :: 3",
    ]
)