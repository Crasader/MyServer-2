#ifndef __DATABASE_QUERYRESULT_H__
#define __DATABASE_QUERYRESULT_H__

#include "field.h"
#include <assert.h>
#include <mysql/mysql.h>
#include <stdint.h>
#include <vector>
#include <map>

class QueryResult
{
public:
	typedef std::map<uint32_t, std::string> FieldNames;

	QueryResult(MYSQL_RES* result, uint64_t rowCount, uint32_t fieldCount);
	virtual ~QueryResult();

	virtual bool NextRow();

	void EndQuery();

	uint32_t GetField_idx(const std::string& name) const
	{
		for (FieldNames::const_iterator iter = GetFieldNames().begin(); iter != GetFieldNames().end(); ++iter)
		{
			if (iter->second == name)
				return iter->first;
		}

		assert(false && "unknown field name");
		return uint32_t(-1);
	}

	const Field & operator [] (int index) const
	{
		return mCurrentRow[index];
	}

	const Field & operator [] (const std::string &name) const
	{
		return mCurrentRow[GetField_idx(name)];
	}

	Field *Fetch() const { return mCurrentRow; }
	uint32_t GetFieldCount() const { return mFieldCount; }
	uint64_t GetRowCount() const { return mRowCount; }
	FieldNames const& GetFieldNames() const { return mFieldNames; }
	vector<string> const& GetNames() const { return m_vtFieldNames; }
private:
	enum Field::DataTypes ConvertNativeType(enum_field_types mysqlType) const;
protected:
	Field *             mCurrentRow;
	uint32_t            mFieldCount;
	uint64_t            mRowCount;
	FieldNames          mFieldNames;
	std::vector<string> m_vtFieldNames;

	MYSQL_RES*          mResult;
};

#endif