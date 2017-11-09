from setuptools import setup

setup(name='cali-interactive-explorer',
      version='0.1',
      description='Interactive exploration tool for Caliper profiles',
      url='http://github.com/LLNL/Caliper',
      author='David Poliakoff',
      author_email='poliakoff1@llnl.gov',
      license='BSD',
      packages=['cali-interactive-explorer'],
      install_requires=[
                    'pandas',
      ],
      zip_safe=False)
