/* Copyright (C) 2010-2021 by Arm Limited. All rights reserved. */

#include "ConfigurationXMLParser.h"

#include <cstdlib>
#include <memory>

static const char TAG_CONFIGURATIONS[] = "configurations";
static const char TAG_CONFIGURATION[] = "configuration";
static const char TAG_SPE[] = "spe";

static const char ATTR_COUNTER[] = "counter";
static const char ATTR_REVISION[] = "revision";
static const char ATTR_EVENT[] = "event";
static const char ATTR_COUNT[] = "count";
static const char ATTR_CORES[] = "cores";

static const char * ATTR_ID = "id";
static const char * ATTR_EVENT_FILTER = "event-filter";
static const char * ATTR_LOAD_FILTER = "load-filter";
static const char * ATTR_STORE_FILTER = "store-filter";
static const char * ATTR_BRANCH_FILTER = "branch-filter";
static const char * ATTR_MIN_LATENCY = "min-latency";

#define CONFIGURATION_REVISION 3
int static configurationsTag(mxml_node_t * node)
{
    const char * revision_string;

    revision_string = mxmlElementGetAttr(node, ATTR_REVISION);
    if (revision_string == nullptr) {
        return 1; //revision issue;
    }

    int revision;
    if (!stringToInt(&revision, revision_string, 10)) {
        LOG_ERROR("Configuration XML revision must be an integer");
        return VERSION_ERROR;
    }
    if (revision < CONFIGURATION_REVISION) {
        LOG_ERROR("Revision issue, please check configuration XML v%d", revision);
        return 1; // revision issue
    }
    // A revision >= CONFIGURATION_REVISION is okay
    // Greater than can occur when Streamline is newer than gator

    return 0;
}
/**
 * parse/read counter and update counter structure
 */
int ConfigurationXMLParser::readCounter(mxml_node_t * node)
{
    int count = -1;
    int cores = -1;
    const char * counterName = mxmlElementGetAttr(node, ATTR_COUNTER);
    const char * eventStr = mxmlElementGetAttr(node, ATTR_EVENT);
    CounterConfiguration counter;
    counter.counterName = counterName;
    if (mxmlElementGetAttr(node, ATTR_COUNT) != nullptr) {
        if (!stringToInt(&count, mxmlElementGetAttr(node, ATTR_COUNT), 10)) {
            LOG_ERROR("Configuration XML count must be an integer");
            return PARSER_ERROR;
        }
        counter.count = count;
    }
    if (mxmlElementGetAttr(node, ATTR_CORES) != nullptr) {
        if (!stringToInt(&cores, mxmlElementGetAttr(node, ATTR_CORES), 10)) {
            LOG_ERROR("Configuration XML cores must be an integer");
            return PARSER_ERROR;
        }
        counter.cores = cores;
    }
    long long event;
    if (eventStr != nullptr) {
        if (!stringToLongLong(&event, eventStr, 16)) {
            LOG_ERROR("Configuration XML event must be an integer");
            return PARSER_ERROR;
        }
        counter.event = EventCode(event);
    }
    counterConfigurations.push_back(counter);
    return 0;
}
/**
 * Parse/read spe elements and update spe structure
 */
int ConfigurationXMLParser::readSpe(mxml_node_t * node)
{
    const char * id = mxmlElementGetAttr(node, ATTR_ID);
    int minLatency = 0;
    SpeConfiguration spe;
    spe.id = id;
    const char * attrEventFilter = mxmlElementGetAttr(node, ATTR_EVENT_FILTER);
    if (attrEventFilter != nullptr) {
        char * end;
        errno = 0;
        uint64_t event_mask = strtoull(attrEventFilter, &end, 0);
        if (event_mask == 0 && end == attrEventFilter) {
            LOG_ERROR("Configuration XML spe event-filter must be an integer");
            return PARSER_ERROR;
        }
        if (event_mask == ULLONG_MAX && errno) {
            LOG_ERROR("Configuration XML spe event-filter must be in the range of unsigned long long");
            return PARSER_ERROR;
        }
        if (*end != 0) {
            LOG_ERROR("Configuration XML spe event-filter must be an integer");
            return PARSER_ERROR;
        }
        spe.event_filter_mask = event_mask;
    }
    const char * attrLoadFilter = mxmlElementGetAttr(node, ATTR_LOAD_FILTER);
    if (attrLoadFilter != nullptr) {
        if (strcmp(attrLoadFilter, "true") == 0) {
            spe.ops.insert(SpeOps::LOAD); //set
        }
        else if (strcmp(attrLoadFilter, "false") != 0) {
            LOG_ERROR("Configuration XML spe load-filter must be either true or false");
            return PARSER_ERROR;
        }
    }
    const char * attrStoreFilter = mxmlElementGetAttr(node, ATTR_STORE_FILTER);
    if (attrStoreFilter != nullptr) {
        if (strcmp(attrStoreFilter, "true") == 0) {
            spe.ops.insert(SpeOps::STORE); //set
        }
        else if (strcmp(attrStoreFilter, "false") != 0) {
            LOG_ERROR("Configuration XML spe store-filter must be either true or false");
            return PARSER_ERROR;
        }
    }
    const char * attrBranchFilter = mxmlElementGetAttr(node, ATTR_BRANCH_FILTER);
    if (attrBranchFilter != nullptr) {
        if (strcmp(attrBranchFilter, "true") == 0) {
            spe.ops.insert(SpeOps::BRANCH); //set
        }
        else if (strcmp(attrBranchFilter, "false") != 0) {
            LOG_ERROR("Configuration XML spe branch-filter must be either true or false");
            return PARSER_ERROR;
        }
    }
    const char * attrMinLatency = mxmlElementGetAttr(node, ATTR_MIN_LATENCY);
    if (attrMinLatency != nullptr) {
        if (!stringToInt(&minLatency, attrMinLatency, 10)) {
            LOG_ERROR("Configuration XML spe min-latency must be an integer");
            return PARSER_ERROR;
        }
        spe.min_latency = minLatency;
    }
    speConfigurations.push_back(spe);
    return 0;
}

/**
 * Parse the xml content passed as argument,
 */
int ConfigurationXMLParser::parseConfigurationContent(const char * config_xml_content)
{
    std::unique_ptr<mxml_node_t, decltype(mxmlDelete) *> tree {
        mxmlLoadString(nullptr, config_xml_content, MXML_NO_CALLBACK),
        &mxmlDelete};
    mxml_node_t * node = mxmlGetFirstChild(tree.get());
    if (node != nullptr) {
        while ((node != nullptr) && mxmlGetType(node) != MXML_ELEMENT) {
            node = mxmlFindElement(node, tree.get(), TAG_CONFIGURATIONS, ATTR_REVISION, nullptr, MXML_NO_DESCEND);
        }
        int revision = configurationsTag(node);
        if (revision != 0) {
            return revision;
        }

        node = mxmlGetFirstChild(node);
        while (node != nullptr) {
            if (mxmlGetType(node) != MXML_ELEMENT) {
                node = mxmlWalkNext(node, tree.get(), MXML_NO_DESCEND);
                continue;
            }
            const char * mxmlGetElement0 = mxmlGetElement(node);
            int read_ret;
            if (strcasecmp(mxmlGetElement0, TAG_SPE) == 0) {
                read_ret = readSpe(node);
            }
            else if (strcasecmp(mxmlGetElement0, TAG_CONFIGURATION) == 0) {
                read_ret = readCounter(node);
            }
            else {
                LOG_DEBUG("Ignoring unknown element while parsing configuration xml (%s)", mxmlGetElement0);
                read_ret = 0;
            }

            if (read_ret == PARSER_ERROR) {
                counterConfigurations.clear();
                speConfigurations.clear();
                LOG_ERROR("Error while parsing configuration xml");
                return PARSER_ERROR;
            }
            node = mxmlWalkNext(node, tree.get(), MXML_NO_DESCEND);
        }
    }
    else {
        LOG_ERROR("Error while parsing configuration xml");
        return PARSER_ERROR;
    }
    return 0;
}

const std::vector<CounterConfiguration> & ConfigurationXMLParser::getCounterConfiguration()
{
    return counterConfigurations;
}
const std::vector<SpeConfiguration> & ConfigurationXMLParser::getSpeConfiguration()
{
    return speConfigurations;
}
