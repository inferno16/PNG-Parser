PNG Parser
==========

A class that provides methods for reading and extracting data from PNG files. The class is currently able to extract the raw pixel data from uncompressed images, but the decompression is still not working, since I don't want to use library for the DEFLATE compression algorithm used in the PNGs.<br>
<br>

Goals:
------
* Get familiar with the Visual Studio's cross-project dependancies (This project uses the class [Binary] as a static library)
* Understand how compression works and in paticular the DEFLATE algorithm
* Create a working PNG reader
* Improve my C++ programming skills

[Binary]: https://github.com/inferno16/BinaryData
