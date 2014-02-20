/*****************************************************************************
 * Copyright (C) 2013 VLC authors and VideoLAN
 *
 * Authors: Nicolas Bertrand <nico@isf.cc>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/**
 * @file dcpparser.h
 * @brief Parse DCP XML files
 */


#ifndef _DCPPARSER_H
#define _DCPPARSER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* VLC core API headers */
#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>

#include <iostream>
#include <string>
#include <list>
#include <vector>

using namespace std;
typedef enum {
    TRACK_UNKNOWN = 0,
    TRACK_PICTURE,
    TRACK_SOUND,
    TRACK_SUBTITLE
} TrackType_t;

typedef enum {
    XML_UNKNOWN = 0,
    XML_ASSETMAP,
    XML_CPL,
    XML_PKL,
    XML_SUB,
} XmlType_t;


class Asset;
class AssetList: public std::list<Asset *> {};
class PKL;

/* This struct stores the most important information about the DCP */
struct dcp_t
{
    string path;                    /* Path to DCP directory */

    vector<PKL *> pkls;
    AssetList *p_asset_list;

    string videofile;               /* Picture file name */
    string audiofile;               /* Sound file name */

    int i_video_entry;              /* Picture entry point */
    int i_audio_entry;              /* Sound entry point */

    dcp_t():
        p_asset_list(NULL),
        i_video_entry(0),
        i_audio_entry(0) {};

    ~dcp_t( ) {
        vlc_delete_all(pkls);
        if ( p_asset_list != NULL ) {
            vlc_delete_all(*p_asset_list);
            delete(p_asset_list);

        }
    }
};

class XmlFile
{
public:
    XmlFile( demux_t * p_demux, string s_path):
    p_demux(p_demux), s_path(s_path),
    p_stream(NULL),
    p_xml(NULL),
    p_xmlReader(NULL),
    type(XML_UNKNOWN) {}

    virtual ~XmlFile( );

    virtual int Parse() = 0;

    bool IsCPL() { return type == XML_CPL; }
protected:
    demux_t      *p_demux;
    string       s_path;
    stream_t     *p_stream;

    xml_t        *p_xml;
    xml_reader_t *p_xmlReader;

    int OpenXml();
    void CloseXml();

    XmlType_t type;
};

class Chunk {
public:
    Chunk(demux_t * demux):
        i_vol_index(1), i_offset(0), i_length(0),
        p_demux(demux) {};
    int Parse(xml_reader_t *p_xmlReader, string p_node, int p_type);
    string getPath() { return this->s_path; };
private:
    string s_path;
    int i_vol_index;
    int i_offset;
    int i_length;
    demux_t      *p_demux;
};

class Asset {
public:
    /* Constructor */
    Asset (demux_t * demux):
        b_is_packing_list(false),  ui_size(0),
        i_intrisic_duration(0), i_entry_point(0), i_duration(0),
        p_demux(demux) {}
    virtual ~Asset() ;

    void setId(string p_string ) { this->s_id = p_string; };
    void setPath(string p_string) { this->s_path = p_string; };
    void setAnnotation(string p_string) {
        if (this->s_annotation.empty())
            this->s_annotation = p_string;
        else
            this->s_annotation = this->s_annotation + "--" + p_string;
    };
    void setPackingList(bool p_bool) { this->s_path = p_bool; };
    void setEntryPoint(int i_val) { this->i_entry_point = i_val; };
    void setDuration (int i_val) { this->i_duration = i_val; };
    void setIntrinsicDuration (int i_val) { this->i_intrisic_duration = i_val; };
    string getId() const { return this->s_id; } ;
    string getPath() const { return this->s_path; };
    string getType() const { return this->s_type; };
    int getEntryPoint() const { return this->i_entry_point; };
    int getDuration() const { return this->i_duration; };
    int getIntrinsicDuration() const { return this->i_intrisic_duration; };

    bool isPackingList() const { return this->b_is_packing_list; };

    int Parse( xml_reader_t *p_xmlReader, string node, int type);
    int ParsePKL( xml_reader_t *p_xmlReader);

    // TODO: remove
    void Dump();

private:
    string      s_id;
    string      s_path;
    string      s_annotation;
    bool        b_is_packing_list;
    string      s_hash;
    uint32_t    ui_size;
    string      s_type;
    string      s_original_filename;
    TrackType_t e_track_type;
    string      s_edit_rate;
    int         i_intrisic_duration;
    int         i_entry_point;
    int         i_duration;
    /* encryption attribute */
    string      s_key_id;
    /* Picture attributes */
    string      s_frame_rate;
    string      s_screen_aspect_ratio;
    /* sound and subtitle */
    string      s_language;

    demux_t     *p_demux;
    std::vector<Chunk> chunk_vec;


    int parseChunkList( xml_reader_t *p_xmlReader, string p_node, int p_type);

};



class Reel
{
public:
    Reel(demux_t * demux, AssetList *asset_list, xml_reader_t *xmlReader)
        : p_asset_list(asset_list), p_xmlReader(xmlReader), p_demux(demux)
         {};
    int Parse(string p_node, int p_type);
    Asset * getTrack(TrackType_t e_track);

private:
    AssetList *p_asset_list;
    xml_reader_t *p_xmlReader;
    demux_t      *p_demux;

    string s_id;
    string s_annotation;
    Asset  *p_picture_track;
    Asset  *p_sound_track;
    Asset  *p_subtitle_track;

    int ParseAssetList(string p_node, int p_type);
    int ParseAsset(string p_node, int p_type, TrackType_t e_track);
};

class CPL : public XmlFile
{
public:
    CPL(demux_t *, string, AssetList*);
    ~CPL();
    virtual int Parse();

    Reel *getReel(int pos) { return this->vec_reel[pos]; } ;

private :
    AssetList *asset_list;

    string s_id;
    string s_annotation;
    string s_icon_id;
    string s_issue_date;
    string s_issuer;
    string s_creator;
    string s_content_title;
    string s_content_kind;
    /* TODO:  ContentVersion, RatingList, signer and signature */

    std::vector<Reel *>   vec_reel;
    int DummyParse(string p_node, int p_type);
    int ParseReelList(string p_node, int p_type);
};


class PKL : public XmlFile
{
public:
    PKL ( demux_t * p_demux, string s_path, AssetList *asset_list,
         string s_dcp_path);
    ~PKL();
    virtual int Parse();

    int FindCPLs();
    CPL *getCPL(int pos) { return this->vec_cpl[pos]; };

private:
    AssetList *asset_list;

    string s_id;
    string s_annotation;
    string s_issue_date;
    string s_issuer;
    string s_creator;
    string s_icon_id;
    string s_group_id;
    string s_dcp_path;
    std::vector<CPL *> vec_cpl;

    int ParseAssetList(string p_node, int p_type);
    int ParseAsset(string p_node, int p_type);
    int ParseSigner(string p_node, int p_type);
    int ParseSignature(string p_node, int p_type);

};

class AssetMap : public XmlFile {

public:
    AssetMap( demux_t * p_demux, string s_path, dcp_t *_p_dcp)
        : XmlFile( p_demux, s_path ), p_dcp( _p_dcp) {};
    ~AssetMap();

    static Asset * getAssetById(AssetList*, const string p_id);

    virtual int Parse();
private:
    dcp_t *p_dcp;

    int ParseAssetList (xml_reader_t *p_xmlReader, const string p_node, int p_type);
};
#endif /* _DCPPARSER_H */
