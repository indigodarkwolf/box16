/*
How does the resampling algorithm work?
=======================================

From the YM2151, we get a signal with a sampling rate of 55930 Hz. The host wants samples at
44100 Hz or 48000 Hz (most likely).

56 kHz
        X-------X-------X-------X-------X-------X-------X-------X-------

44 kHz
        X---------X---------X---------X---------X---------X---------X---------

In order to understand how to "correctly" resample the signal from the 55930 Hz that the YM2151
outputs to the 44100 Hz or 48000 Hz, we need to consider the Nyquist-Shannon theorem. It states
that every continuous signal (which obeys a certain condition) can be represented as a series of
time-discrete values (samples), and the underlying contiguous signal can be recovered perfectly
from these samples.
The condition for this to work is that the fourier transform of the continuous signal does not
contain any content above half the sampling frequency -- the so-called Nyquist frequency.
In return, if we reconstruct a continuous signal from discrete samples, it will never contain
any content above the Nyquist frequency, but it may contain content up to it.

When doing dowsnsampling, as we intend to do it, this poses a problem: the Nyquist frequency of
the YM2151 is 28 kHz, and the host's is 22 kHz. That means, in general, what comes out of the
YM2151 cannot be represented in the host's sampling rate. First, the content above 22 kHz must be
removed from the YM2151's signal, before we "are allowed to" take samples from this signal at
discrete points in time.

Of course, we could still sample a signal with content above the Nyquist frequency. However, this
"forbidden" content will then appear in the intended sampling range as unwanted noise. This
phenomenon is called aliasing. To minimise aliasing, the original signal must be filtered to the
allowed range.

When we have a filtered signal, we still need to downsample it. Downsampling by an integer factor
is easy: just keep every Nth sample. Downsampling by a rational fraction can be done by upsampling
by a factor of M and then downsampling by a factor of N. Now, using this approach would require us
to upsample by a ridiculous amount that is not even known at compile time (due to the host's
sampling fequency being unknown). We therefore choose a crude approach: we still upsample by an
integer factor, but during downsampling, we choose irregular strides for picking the samples we
keep. Sometimes we leave N-1 samples away in between, and sometimes N, so that the average rate is
correct. This of course introduces some error, but it can be kept small by choosing an
appropriately large upsampling factor.

Upsampling by a factor of M is done by inserting M-1 zeros in between two original samples, and
running that signal through a low-pass filter to interpolate and smoothen out the signal.

This "interpolation filter" and the anti-aliasing filter mentioned earlier can be one and the same
filter! The filter will be implemented as a finite impulse response filter (FIR filter). More
information about FIR filters can be found in one of the links below. Essentially, every output
sample of an FIR filter is a weighted sum of the current and a number of the previous input samples
of the filter. Because many of these samples are zero (and we know which ones!), we can shrink this
sum drastically to reduce computational cost.

There is one problem with the "inserting zeros" approach, and that is that a signal that contains a
series of sharp peaks contains a LOT of high frequency content! In fact, if one were to listen to
such a signal (and the high frequency content would not be above the audible range), the high
frequency content would completely dominate the listening experience.
FIR filters do not completely eliminate unwanted frequencies, but they reduce their amplitude. Now,
if the signal contains so much dominating high frequency, this puts tough requirements on the
interpolation filter. The attenuation for its stop-band (the frequencies it is meant to filter out)
must be very high in order to achieve acceptable quality.
Instead, we can do something called zero-order-hold, which is basically creating a stairstep
pattern instead of a series of sharp peaks and zeros in between. In such a signal, the unwanted
high frequency content is much lower in amplitude and therefore easier to suppress to an acceptable
level.
Luckily, this operation (creating stairsteps from peaks) can be included into the FIR filter. No
further operation required, and we can still benefit from the optimization obtained from assuming
that only every Mth sample is non-zero.

Furthermore, we do not need the entire upsampled signal, since we are leaving away most of the
samples anyway and are very "picky" about which ones to keep. This means we don't need to compute
most of the upsampled signal, in the first place. This adds another layer of cost savings.

So, to recap, we do the following steps:
* upsample the input signal by a factor of M by inserting M-1 zeros between input samples.
* apply a filter to smoothen out the signal and remove content above the target's Nyquist frequency
* pick every Nth sample (where N can be a non-integer number. choose the closest one in that case)


With the optimizations applied, this boils down to the following approach:
* For every output sample, compute output sample:
  * choose the sample index in the upsampled signal that is closest to the desired point in time where the "current output sample" is supposed to occur
  * determine which input sample that corresponds to
    * compute the weighted sum over the previous X input samples (depending on the filter layout)



Further watching and reading:
=============================

An excellent introduction into digital sampling:
https://youtu.be/cIQ9IXSUzuM

Nyquist-Shannon theorem and aliasing:
https://youtu.be/FcXZ28BX-xE?t=251

FIR filters:
https://youtu.be/uNNNj9AZisM

Downsampling:
https://en.wikipedia.org/wiki/Downsampling_(signal_processing)

Upsampling:
https://dsp.stackexchange.com/questions/3614/what-are-the-relative-merits-of-various-upsampling-schemes

Interpolation trick:
Question: https://www.kvraudio.com/forum/viewtopic.php?p=2415182#p2415182
Answer: https://www.kvraudio.com/forum/viewtopic.php?p=2415353#p2415353






How are the filter coefficients being computed?
===============================================

I used the following website
http://t-filter.engineerjs.com/

to generate coefficients for a low-pass filter. I then post-processed them to apply the "zero
-order-hold". This post-processing consists of simply adding up several shifted copies of the
coefficients, the upsampling factor M being the number of copies being added up. In the following
example, the upsampling factor would be 4, and [h1 h2 ...] would be the coefficients obtained from
aforementioned website.

[h'1 h'2 h'3 h'4 ... ] = [h1 h2 h3 h4 ... 0 0 0] + [0 h1 h2 h3 h4 ... 0 0] + [0 0 h1 h2 h3 h4 ... 0] + [0 0 0 h1 h2 h3 h4 ...]

By this operation, there will be M-1 more coefficients in the end than there were to start with.

Choosing the settings of the filter is a delicate problem, as improving the quality of the filtered
sound generally increases unwanted latency (delay in the audio signal).

The settings chosen for the filter are shown below. The rationale behind it is:
The sampling frequency is the input frequency times the upsampling factor: 55930 Hz * 8 = 447440 Hz

The pass-band was chosen to go "only" to 19 kHz as a compromise:
* 19 kHz is higher than many can hear anyway
* lower frequency allows for greater transition band, which decreases required filter length

The ripple should be set as low as possible to reduce unwanted colorization of the sound,
but should be allowed high enough to reduce required filter length (a tradeoff).

The stop-band was chosen to start at 24 kHz.
* it is low enough to have most unwanted content removed, even if the sampliing frequency is only 44 kHz (nyquist = 22 kHz)
* it is high enough to reduce the required filter length.

The desired attenuation is set low enough to yield acceptable sound quality, yet high enough to allow for reasonably short filter length.






sampling frequency: 447440 Hz

* 0 Hz - 19000 Hz
  gain = 1
  desired ripple = 0.9 dB
  actual ripple = 0.6409619042605599 dB

* 24050 Hz - 223720 Hz
  gain = 0
  desired attenuation = -20 dB
  actual attenuation = -20.452928024179858 dB

*/

static constexpr int filter_kernel_length = 94;
static constexpr double filter_kernel[filter_kernel_length] = 
{
    0.044681718219612455,
    0.038937756198241275,
    0.032924091027748625,
    0.026353416072323117,
    0.0190943154448875,
    0.01118674446557677,
    0.0028318760749698746,
    -0.00560360390338463,
    -0.058325743471175535,
    -0.059671460841642994,
    -0.059229911151506554,
    -0.05619763883362015,
    -0.050033289571915565,
    -0.04054889386001541,
    -0.02791568338203053,
    -0.012730916280099978,
    0.004046244468129244,
    0.021120265877148634,
    0.03695092254107014,
    0.05014333444240057,
    0.059092109836150014,
    0.06256489772863927,
    0.059644199525041956,
    0.049924087842381004,
    0.033593193473589605,
    0.011492692325992284,
    -0.014847846671535296,
    -0.043547186979742344,
    -0.07197564107319891,
    -0.0973303117533685,
    -0.11666155955647083,
    -0.12711081455944076,
    -0.1261820314043462,
    -0.11189332373667972,
    -0.08302365799937911,
    -0.03922200828573366,
    0.018892503806521334,
    0.08966857254312538,
    0.17058734213780602,
    0.2582574216302505,
    0.348705822176801,
    0.4375404312507989,
    0.5202640656994062,
    0.592552421233066,
    0.6505236415781948,
    0.6910597885079295,
    0.7118976515654749,
    0.7118976515654748,
    0.6910597885079294,
    0.6505236415781948,
    0.592552421233066,
    0.5202640656994062,
    0.4375404312507989,
    0.348705822176801,
    0.2582574216302505,
    0.17058734213780602,
    0.08966857254312538,
    0.018892503806521327,
    -0.03922200828573366,
    -0.0830236579993791,
    -0.11189332373667972,
    -0.1261820314043462,
    -0.12711081455944076,
    -0.11666155955647083,
    -0.09733031175336848,
    -0.07197564107319893,
    -0.04354718697974233,
    -0.014847846671535287,
    0.011492692325992277,
    0.033593193473589605,
    0.049924087842381,
    0.05964419952504195,
    0.06256489772863928,
    0.059092109836150014,
    0.050143334442400565,
    0.036950922541070144,
    0.02112026587714863,
    0.004046244468129239,
    -0.012730916280099976,
    -0.02791568338203053,
    -0.04054889386001542,
    -0.050033289571915565,
    -0.05619763883362015,
    -0.05922991115150655,
    -0.05967146084164298,
    -0.058325743471175535,
    -0.005603603903384623,
    0.0028318760749698694,
    0.011186744465576768,
    0.0190943154448875,
    0.026353416072323117,
    0.03292409102774862,
    0.038937756198241275,
    0.044681718219612455
};