# UUXCOMP

UUXCOMP provides email compression solution based on UUCP.
Hermes UUXCOMP is uux wrapper meant to be used in postfix as drop-in replacement to uux command. It is inspired by the original UUCOMP implementation by Edwin R. Carp.

Source location of original uucomp is: https://opensource.apple.com/source/uucp/uucp-13/uucp/contrib/uucomp.shar.auto.html

# Features

Rhizomatica's uuxcomp provides rewriting features for email's audio and image
attachments, which includes media transcoding (scripts available in https://github.com/Rhizomatica/hermes-net/tree/main/system_scripts/compression ), and compression (gzip and lzma supported). The media codecs packages are available at:
http://packages.hermes.radio/

Currently are in use the H.266 (VVC) for image compression and LPCNET for audio compression.

# Usage

Postfix uucp configuration should be changed to use uuxcomp instead of uux, and rmail should be changed to crmail. Also, crmail must be put in the uucp configuration
as a authorized command to be executed.

## License

GPL v3+

## Author

Rafael Diniz <rafael@rhizomatica.org>
