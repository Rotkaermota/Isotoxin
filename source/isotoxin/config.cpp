#include "isotoxin.h"

ts::static_setup<config_c,1000> cfg;

config_base_c::~config_base_c()
{
}

DWORD config_base_c::handler_SEV_CLOSE(const system_event_param_s & p)
{
    onclose();
    gui->delete_event(DELEGATE(this, save_dirty));
    save_dirty(RID(), GUIPARAM(1));
    closed = true;
    return 0;
}

void config_base_c::prepare_conf_table( ts::sqlitedb_c *db )
{
    if (ASSERT(db)/* && !db->is_table_exist(CONSTASTR("conf"))*/)
    {
        ts::column_desc_s cfgcols[2];
        cfgcols[0].name_ = CONSTASTR("name");
        cfgcols[0].primary = true;
        cfgcols[0].type_ = ts::data_type_e::t_str;

        cfgcols[1].name_ = CONSTASTR("value");
        cfgcols[1].type_ = ts::data_type_e::t_str;

        //db->create_table(CONSTASTR("conf"), ARRAY_WRAPPER(cfgcols), true);
        db->update_table_struct(CONSTASTR("conf"), ARRAY_WRAPPER(cfgcols), true);
        
    }

}
bool config_base_c::save_dirty(RID, GUIPARAM save_all_now)
{
    bool some_data_still_not_saved = save();
    for(;save_all_now && some_data_still_not_saved; some_data_still_not_saved = save()) ;
    if (some_data_still_not_saved) DELAY_CALL_R( 1.0, DELEGATE(this, save_dirty), nullptr );
    return true;
}
bool config_base_c::save()
{
    if (db && dirty.size())
    {
        ts::str_c vn = dirty.last();
        ts::data_pair_s d[2];
        d[0].name = CONSTASTR("name");
        d[0].text = vn;
        d[0].type_ = ts::data_type_e::t_str;

        d[1].name = CONSTASTR("value");
        d[1].text = values[vn];
        d[1].type_ = ts::data_type_e::t_str;

        db->insert(CONSTASTR("conf"), ARRAY_WRAPPER(d));

        dirty.truncate(dirty.size() - 1);
        while (dirty.remove_fast(vn));
    }
    return dirty.size() != 0;
}

bool config_base_c::param( const ts::asptr& pn, const ts::asptr& vl )
{
    if (closed) return false;
    bool added = false;
    auto &v = values.addAndReturnItem(pn, added);
    if (added || !v.value.equals(vl))
    {
        v.value = vl;
        dirty.add(v.key);
        changed();
        return true;
    }
    return false;
}

void config_base_c::changed(bool save_all_now)
{
    DELAY_CALL_R(1.0, DELEGATE(this, save_dirty), save_all_now ? (GUIPARAM)1 : nullptr);
}

bool find_config(ts::wstr_c &path)
{
    path = ts::get_full_path<ts::wchar>(CONSTWSTR("config.db"));
    bool found_cfg = ts::is_file_exists<ts::wchar>(path);
    if (!found_cfg)
    {
        path = ts::get_full_path(ts::wstr_c(CONSTWSTR("%APPDATA%\\isotoxin\\config.db")), true, true);
        found_cfg = ts::is_file_exists<ts::wchar>(path);
    }
    return found_cfg;
}


void config_c::load( bool config_must_be_present )
{
    ASSERT(g_app);

    bool found_cfg = find_config(path);

    if (!found_cfg) 
    {
        if (config_must_be_present)
        {
            ERROR("Config MUST BE PRESENT");
            sys_exit(0);
            return;
        }
        // no config :(
        // aha! 1st run!
        // show dialog
        gui->load_theme(CONSTWSTR("def"));
        SUMMON_DIALOG<dialog_firstrun_c>();

    } else
    {
        db = ts::sqlitedb_c::connect(path);
    }

    if (db)
    {
        db->read_table( CONSTASTR("conf"), get_cfg_reader() );
    }

}

bool config_base_c::cfg_reader( int row, ts::SQLITE_DATAGETTER getta )
{
    ts::data_value_s v;
    if (CHECK( ts::data_type_e::t_str == getta(0, v) ))
    {
        ts::str_c &vv = values[v.text];
        if (CHECK( ts::data_type_e::t_str == getta(1, v) ))
            vv = v.text;
    }
    return true;
}