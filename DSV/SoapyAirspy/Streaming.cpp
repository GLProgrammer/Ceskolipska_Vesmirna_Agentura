/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "SoapyAirspy.hpp"
#include <SoapySDR/Logger.hpp>
#include <SoapySDR/Formats.hpp>
#include <algorithm> //min
#include <climits> //SHRT_MAX
#include <cstring> // memcpy


std::vector<std::string> SoapyAirspy::getStreamFormats(const int direction, const size_t channel) const {
    std::vector<std::string> formats;

    // formats.push_back("CS8");
    formats.push_back(SOAPY_SDR_CS16);
    formats.push_back(SOAPY_SDR_CF32);

    return formats;
}

std::string SoapyAirspy::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const {
     fullScale = 32767;
     return SOAPY_SDR_CS16;
}

SoapySDR::ArgInfoList SoapyAirspy::getStreamArgsInfo(const int direction, const size_t channel) const {
    SoapySDR::ArgInfoList streamArgs;

    // SoapySDR::ArgInfo chanArg;
    // chanArg.key = "chan";
    // chanArg.value = "mono_l";
    // chanArg.name = "Channel Setup";
    // chanArg.description = "Input channel configuration.";
    // chanArg.type = SoapySDR::ArgInfo::STRING;
    // std::vector<std::string> chanOpts;
    // std::vector<std::string> chanOptNames;
    // chanOpts.push_back("mono_l");
    // chanOptNames.push_back("Mono Left");
    // chanOpts.push_back("mono_r");
    // chanOptNames.push_back("Mono Right");
    // chanOpts.push_back("stereo_iq");
    // chanOptNames.push_back("Complex L/R = I/Q");
    // chanOpts.push_back("stereo_qi");
    // chanOptNames.push_back("Complex L/R = Q/I");
    // chanArg.options = chanOpts;
    // chanArg.optionNames = chanOptNames;
    // streamArgs.push_back(chanArg);

    return streamArgs;
}

/*******************************************************************
 * Async thread work
 ******************************************************************/

static int _rx_callback(airspy_transfer *t)
{
    //printf("_rx_callback\n");
    SoapyAirspy *self = (SoapyAirspy *)t->ctx;
    return self->rx_callback(t);
}

int SoapyAirspy::rx_callback(airspy_transfer *t)
{
    if (sampleRateChanged.load()) {
        return 1;
    }

    //printf("_rx_callback %d _buf_head=%d, numBuffers=%d\n", len, _buf_head, _buf_tail);
    //overflow condition: the caller is not reading fast enough
    if (_buf_count == numBuffers)
    {
        _overflowEvent = true;
        return 0;
    }

    //copy into the buffer queue
    auto &buff = _buffs[_buf_tail];
    buff.resize(t->sample_count * bytesPerSample);
    std::memcpy(buff.data(), t->samples, t->sample_count * bytesPerSample);

    //increment the tail pointer
    _buf_tail = (_buf_tail + 1) % numBuffers;

    //increment buffers available under lock
    //to avoid race in acquireReadBuffer wait
    {
        std::lock_guard<std::mutex> lock(_buf_mutex);
        _buf_count++;
    }

    //notify readStream()
    _buf_cond.notify_one();

    return 0;
}

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDR::Stream *SoapyAirspy::setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args)
{
    //check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) != 0)) {
        throw std::runtime_error("setupStream invalid channel selection");
    }

    airspy_sample_type asFormat = AIRSPY_SAMPLE_INT16_IQ;

    //check the format
    if (format == SOAPY_SDR_CF32) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
        asFormat = AIRSPY_SAMPLE_FLOAT32_IQ;
    } else if (format == SOAPY_SDR_CS16) {
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
        asFormat = AIRSPY_SAMPLE_INT16_IQ;
    } else {
        throw std::runtime_error(
                "setupStream invalid format '" + format
                        + "' -- Only CS16 and CF32 are supported by SoapyAirspy module.");
    }

    airspy_set_sample_type(dev, asFormat);
    sampleRateChanged.store(true);

    bytesPerSample = SoapySDR::formatToSize(format);

    //We get this many complex samples over the bus.
    //Its the same for both complex float and int16.
    //TODO adjust when packing is enabled
    bufferLength = DEFAULT_BUFFER_BYTES/4;

    //clear async fifo counts
    _buf_tail = 0;
    _buf_count = 0;
    _buf_head = 0;

    //allocate buffers
    _buffs.resize(numBuffers);
    for (auto &buff : _buffs) buff.reserve(bufferLength*bytesPerSample);
    for (auto &buff : _buffs) buff.resize(bufferLength*bytesPerSample);

    return (SoapySDR::Stream *) this;
}

void SoapyAirspy::closeStream(SoapySDR::Stream *stream)
{
    _buffs.clear();
}

size_t SoapyAirspy::getStreamMTU(SoapySDR::Stream *stream) const
{
    return bufferLength;
}

int SoapyAirspy::activateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs,
        const size_t numElems)
{
    if (flags != 0) {
        return SOAPY_SDR_NOT_SUPPORTED;
    }
    
    resetBuffer = true;
    bufferedElems = 0;
    
    if (sampleRateChanged.load()) {
        airspy_set_samplerate(dev, sampleRate);
        sampleRateChanged.store(false);
    }
    airspy_start_rx(dev, &_rx_callback, (void *) this);
    
    return 0;
}

int SoapyAirspy::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (flags != 0) return SOAPY_SDR_NOT_SUPPORTED;

    airspy_stop_rx(dev);
    
    streamActive = false;
    
    return 0;
}

int SoapyAirspy::readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs)
{    
    if (!airspy_is_streaming(dev)) {
        return 0;
    }
    
    if (sampleRateChanged.load()) {
        airspy_stop_rx(dev);
        airspy_set_samplerate(dev, sampleRate);
        airspy_start_rx(dev, &_rx_callback, (void *) this);
        sampleRateChanged.store(false);
    }

    //this is the user's buffer for channel 0
    void *buff0 = buffs[0];

    //are elements left in the buffer? if not, do a new read.
    if (bufferedElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, _currentHandle, (const void **)&_currentBuff, flags, timeNs, timeoutUs);
        if (ret < 0) return ret;
        bufferedElems = ret;
    }

    size_t returnedElems = std::min(bufferedElems, numElems);

    //convert into user's buff0
    std::memcpy(buff0, _currentBuff, returnedElems * bytesPerSample);
    
    //bump variables for next call into readStream
    bufferedElems -= returnedElems;
    _currentBuff += returnedElems * bytesPerSample;

    //return number of elements written to buff0
    if (bufferedElems != 0) flags |= SOAPY_SDR_MORE_FRAGMENTS;
    else this->releaseReadBuffer(stream, _currentHandle);
    return returnedElems;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapyAirspy::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    return _buffs.size();
}

int SoapyAirspy::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    buffs[0] = (void *)_buffs[handle].data();
    return 0;
}

int SoapyAirspy::acquireReadBuffer(
    SoapySDR::Stream *stream,
    size_t &handle,
    const void **buffs,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    //reset is issued by various settings
    //to drain old data out of the queue
    if (resetBuffer)
    {
        //drain all buffers from the fifo
        _buf_head = (_buf_head + _buf_count.exchange(0)) % numBuffers;
        resetBuffer = false;
        _overflowEvent = false;
    }

    //handle overflow from the rx callback thread
    if (_overflowEvent)
    {
        //drain the old buffers from the fifo
        _buf_head = (_buf_head + _buf_count.exchange(0)) % numBuffers;
        _overflowEvent = false;
        SoapySDR::log(SOAPY_SDR_SSI, "O");
        return SOAPY_SDR_OVERFLOW;
    }

    //wait for a buffer to become available
    if (_buf_count == 0)
    {
        std::unique_lock <std::mutex> lock(_buf_mutex);
        _buf_cond.wait_for(lock, std::chrono::microseconds(timeoutUs), [this]{return _buf_count != 0;});
        if (_buf_count == 0) return SOAPY_SDR_TIMEOUT;
    }

    //extract handle and buffer
    handle = _buf_head;
    _buf_head = (_buf_head + 1) % numBuffers;
    buffs[0] = (void *)_buffs[handle].data();
    flags = 0;

    //return number available
    return _buffs[handle].size() / bytesPerSample;
}

void SoapyAirspy::releaseReadBuffer(
    SoapySDR::Stream *stream,
    const size_t handle)
{
    //TODO this wont handle out of order releases
    _buf_count--;
}
