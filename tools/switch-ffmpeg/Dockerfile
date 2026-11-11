# The Switch build image: the pinned devkitPro image with ffmpeg rebuilt for https (see build.sh).
#   docker build -t nuvioswitch-build tools/switch-ffmpeg
FROM devkitpro/devkita64:20260219
COPY build.sh /tmp/build.sh
RUN /tmp/build.sh /tmp/switch-ffmpeg && rm -rf /tmp/switch-ffmpeg /tmp/build.sh
