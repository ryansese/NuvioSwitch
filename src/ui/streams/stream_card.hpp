#pragma once

#include <borealis.hpp>
#include <functional>

#include "domain/stream.hpp"

// One stream in the picker: its name, up to three lines of description, and the add-on
// and file size. A stream that can't play is dimmed and labelled "Needs debrid" or "Unsupported".
class StreamCard : public brls::Box
{
  public:
    StreamCard();

    void setStream(const nuvio::domain::Stream& stream);
    const nuvio::domain::Stream& getStream() const { return stream; }

    std::function<void(StreamCard*)> onSelect;

  private:
    nuvio::domain::Stream stream;

    BRLS_BIND(brls::Label, name, "stream/name");
    BRLS_BIND(brls::Label, badge, "stream/badge");
    BRLS_BIND(brls::Label, description, "stream/description");
    BRLS_BIND(brls::Label, meta, "stream/meta");
};
