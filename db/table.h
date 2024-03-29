#pragma once
#include <memory>
#include <functional>
#include "descriptor.h"
#include "rocks_wrapper.h"
// #include "rocksdb/db.h"

namespace rocksdb{
    class Iterator;
}

namespace aquadb
{

#define MY_DISABLE_COPY(CLASS_NAME)  CLASS_NAME(const CLASS_NAME&) = delete; CLASS_NAME& operator=(const CLASS_NAME&) = delete

class TableRowView
{
    public:

};


class TableRow
{
    public:
    TableRow() = default;
    ~TableRow() = default;
    int append(const std::string& name, const Value& val)
    {
        // FIXME 
        // to check name exists
        // TODO
        names_.push_back(name);
        values_.push_back(val);
        return 0;
    }
    

    const std::string& name(int offset) const {return names_.at(offset);}
    int type(int offset) const {return values_.at(offset).dtype();}
    const Value& value(int offset) const { return values_.at(offset);}
    const Value& value(const std::string& name) const { 
        auto it = std::find(names_.cbegin(), names_.cend(), name);
        if(it == names_.cend())
         {
             throw std::logic_error("not found the column");
         }
         auto offset = std::distance(names_.cbegin(), it);
        return values_.at(offset);
        }
    void clear() { names_.clear(); values_.clear();}
    size_t size() const {return values_.size();}
    private:
    std::vector<std::string> names_;
    std::vector<Value> values_;
    //TupleObject record_;
};

class TableRowSet{
    public:
    TableRowSet(const std::vector<std::string>& names): names_(names)
    {
    }
    TableRowSet(std::vector<std::string>&& names): names_(names)
    {
    }
    ~TableRowSet() = default;

    void append(const TableRow& row)
    {
        auto sz = row.size();
        for(size_t i =0;i < sz ; ++i)
        {
            auto const& name = row.name(i);
            auto it = std::find(names_.cbegin(), names_.cend(), name);
            // TODO
            // check it 
            auto pos = std::distance(names_.cbegin(), it);
            values_[pos].push_back(row.value(i));
        }
    }
    size_t column_num() const { return values_.size();}
    size_t rows_num() const { return values_.empty() ? 0: values_.cbegin()->size();}

    const std::string& name(int offset) const {return names_.at(offset);}
    int type(size_t rowid,int offset) const {return values_.at(rowid).at(offset).dtype();}
    const Value& value(size_t rowid, int offset) const { return values_.at(rowid).at(offset);}
    const Value& value(size_t rowid,const std::string& name) const { 
        auto it = std::find(names_.cbegin(), names_.cend(), name);
        if(it == names_.cend())
         {
             throw std::logic_error("not found the column");
         }
         auto offset = std::distance(names_.cbegin(), it);
        return values_.at(rowid).at(offset);
        }
    public:
    std::vector<std::string> names_;
    std::vector<std::vector<Value>> values_;
};

class TableOperator
{
    public:
    TableOperator(DatabaseDescriptorPtr db, TableDescriptorPtr tbl, RocksWrapper* wrapper):_db(db),_tbl(tbl),_wrapper(wrapper) {}
    ~TableOperator() = default;

    int put(const Value& key, const Value& val);
    int get(const Value& key, Value& val);
    int get_range(const Value& start_key, const Value& end_key, TableRowSet& rows);

    int scan_range(const Value& start_key, const Value& end_key, std::function<bool(const TableRow& row)> fn);
    int scan_prefix(const Value& prefix, std::function<bool(const TableRow& row)> fn);

    int get_first(TableRow& record);
    int get_last(TableRow& record);

    int insert(const TableRow& row);
    int insert(const TableRowSet& rows);

    //int replace(const TableRecord& record);
    //int replace(const std::vector<TableRecord>& records);

    int remove(const Value& key);
    int remove_range(const Value& start_key, const Value& end_key);
    int remove(const std::vector<Value>& row);

    MutTableKey build_table_key(const Value& value);
    // 用原始数据构建key
    MutTableKey build_table_key_raw(const Value& value);
    MutTableKey build_table_key(const std::vector<Value>& values);

    MutTableKey build_table_key(const TableRow& row);
    MutTableKey build_table_key(const TableRowSet& rows, size_t rowid);

    BufferArray build_table_value(const TableRow& row);
    BufferArray build_table_value(const TableRowSet& rows, size_t rowid);
private:
    MY_DISABLE_COPY(TableOperator);

    private:
    DatabaseDescriptorPtr _db;
    TableDescriptorPtr _tbl;
    RocksWrapper* _wrapper;
    std::string _errmsg;
};
typedef std::shared_ptr<TableOperator> TableOperatorPtr;

class TableReader
{
    public:
    TableReader(DatabaseDescriptorPtr db, TableDescriptorPtr tbl,RocksWrapper* wrapper):_db(db),_tbl(tbl),_wrapper(wrapper) {}
    ~TableReader() = default;

    int seek_to_first(const IndexDescriptor* index, const std::vector<Value>& key);
    inline int seek_to_first() {
         std::vector<Value> key;
        return seek_to_first(_tbl->get_primary_key(), key);
    }

    int seek_to_last(const IndexDescriptor* index, const std::vector<Value>& key);
    inline int seek_to_last()
    {
        std::vector<Value> key;
        return seek_to_first(_tbl->get_primary_key(), key);
    }

    int seek_to(const IndexDescriptor* index,  const std::vector<Value>& key,bool prefix = false);
    inline int seek_to(const std::vector<Value>& key,bool prefix = false) { return seek_to(_tbl->get_primary_key(), key, prefix);}

    int next();
    int prev();

    int get(const std::vector<Value>& pk, std::vector<Value>& row);
    int get(const Value& key, Value& val);
    int get(const std::vector<Value>& pk, TupleObject& record);
    inline int get(Value& key, TupleObject& record) { return get({key}, record);}

    inline const std::vector<std::string>& get_columns() const { return _names;}
    inline const std::vector<Value>& get_current_row() const { return _values;}
    inline const std::string& get_errmsg() const { return _errmsg;}
private:
    MY_DISABLE_COPY(TableReader);

    private:
    DatabaseDescriptorPtr _db;
    TableDescriptorPtr _tbl;
    RocksWrapper* _wrapper;
    std::unique_ptr<rocksdb::Iterator> _iter{nullptr};
    MutTableKey _mutkey;
    std::vector<std::string> _names;
    std::vector<Value> _values;
    TupleObject _record;
    std::string _errmsg;
};
typedef std::shared_ptr<TableReader> TableReaderPtr;

}