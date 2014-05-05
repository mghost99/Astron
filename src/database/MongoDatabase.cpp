#include "DatabaseBackend.h"
#include "DBBackendFactory.h"
#include "DatabaseServer.h"

#include "core/global.h"
#include "util/DatagramIterator.h"
#include "util/Datagram.h"

#include <client/dbclient.h>
#include <bson/bson.h>

using namespace std;
using namespace mongo;

static ConfigVariable<string> server("server", "localhost", db_backend_config);
static ConfigVariable<string> database("database", "test", db_backend_config);
static ConfigVariable<string> username("username", "", db_backend_config);
static ConfigVariable<string> password("password", "", db_backend_config);

// These are helper functions to convert between BSONElement and packed Bamboo
// field values.

// TODO: Right now they just store the packed Bamboo field data in a BSON blob
//       verbatim. They need to do an in-depth conversion so that the database
//       is accessible by other tools and tolerant of changes to the .dc file.
static void unpack_bson(const dclass::Field *field,
                        const std::vector<uint8_t> data,
                        BSONObjBuilder &ob)
{
	ob.appendBinData(field->get_name(), data.size(), BinDataGeneral, data.data());
}

static void pack_bson(const dclass::Field *field,
                      const BSONElement &element,
                      std::vector<uint8_t> &data)
{
	int len;
	const char *rawdata = element.binData(len);
	data.resize(len);
	memcpy(data.data(), rawdata, len);
}

class MongoDatabase : public DatabaseBackend
{
	public:
		MongoDatabase(ConfigNode dbeconfig, doid_t min_id, doid_t max_id) :
			DatabaseBackend(dbeconfig, min_id, max_id), m_monotonic_exhausted(false)
		{
			stringstream log_name;
			log_name << "Database-MongoDB" << "(Range: [" << min_id << ", " << max_id << "])";
			m_log = new LogCategory("mongodb", log_name.str());

			// Init connection.
			string error;

			// TODO: This only creates a single connection. When this class is
			// made multithreaded, we will need a connection pool instead.
			if(!m_conn.connect(server.get_rval(m_config), error))
			{
				m_log->fatal() << "Connection failure: " << error << endl;
				exit(1);
			}

			m_db = database.get_rval(m_config);

			string u = username.get_rval(m_config);
			string p = password.get_rval(m_config);
			if(!u.empty() && !p.empty())
			{
				if(!m_conn.auth(m_db, u, p, error))
				{
					m_log->fatal() << "Authentication failure: " << error << endl;
					exit(1);
				}
			}

			// Init the collection names:
			stringstream obj_collection;
			obj_collection << m_db << ".astron.objects";
			m_obj_collection = obj_collection.str();
			stringstream global_collection;
			global_collection << m_db << ".astron.globals";
			m_global_collection = global_collection.str();

			// Init the globals collection/document:
			BSONObj globals = BSON("_id" << "GLOBALS" <<
			                       "doid" << BSON(
				                       "monotonic" << min_id <<
				                       "free" << BSONArray()
			                       ));
			m_conn.insert(m_global_collection, globals);
		}

		virtual void submit(DBOperation *operation)
		{
			// TODO: This should run in a separate thread.
			handle_operation(operation);
		}

	private:
		LogCategory *m_log;

		DBClientConnection m_conn;
		string m_db;
		string m_obj_collection;
		string m_global_collection;

		// N.B. this variable is NOT guarded by a lock. While there can conceivably
		// be races on accessing it, this is not a problem, because:
		// 1) It is initialized to false by the main thread, and only set to true
		//    by sub-threads. There is no way for this variable to go back from
		//    true to false.
		// 2) It is only used to tell the DOID allocator to stop trying to use the
		//    monotonic counter. If a thread misses the update from false->true,
		//    it will only waste time fruitlessly trying to allocate an ID from
		//    the (exhausted) monotonic counter, before falling back on the free
		//    DOIDs list.
		bool m_monotonic_exhausted;

		void handle_operation(DBOperation *operation)
		{
			// First, figure out what kind of operation it is, and dispatch:
			if(operation->m_type == DBOperation::OperationType::CREATE_OBJECT)
			{
				handle_create(operation);
			}
			else if(operation->m_type == DBOperation::OperationType::DELETE_OBJECT)
			{
				handle_delete(operation);
			}
			else if(operation->m_type == DBOperation::OperationType::GET_OBJECT ||
			        operation->m_type == DBOperation::OperationType::GET_FIELDS)
			{
				handle_get(operation);
			}
			else if(operation->m_type == DBOperation::OperationType::MODIFY_FIELDS)
			{
				handle_modify(operation);
			}
		}

		void handle_create(DBOperation *operation)
		{
			// First, let's convert the requested object into BSON; this way, if
			// a failure happens, it happens before we waste a doid.
			BSONObjBuilder fields;

			try
			{
				for(auto it = operation->m_set_fields.begin();
				    it != operation->m_set_fields.end();
				    ++it)
				{
					unpack_bson(it->first, it->second, fields);
				}
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "While formatting "
				               << operation->m_dclass->get_name()
				               << " for insertion: " << e.what() << endl;
				operation->on_failure();
				return;
			}

			doid_t doid = assign_doid();
			if(doid == INVALID_DO_ID)
			{
				// The error will already have been emitted at this point, so
				// all that's left for us to do is fail silently:
				operation->on_failure();
				return;
			}

			BSONObj b = BSON("_id" << doid <<
			                 "dclass" << operation->m_dclass->get_name() <<
			                 "fields" << fields.obj());

			m_log->trace() << "Inserting new " << operation->m_dclass->get_name()
				           << "(" << doid << "): " << b << endl;

			try
			{
				m_conn.insert(m_obj_collection, b);
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Cannot insert new "
				               << operation->m_dclass->get_name()
				               << "(" << doid << "): " << e.what() << endl;
				operation->on_failure();
				return;
			}

			operation->on_complete(doid);
		}

		void handle_delete(DBOperation *operation)
		{
			BSONObj result;

			bool success;
			try
			{
				success = m_conn.runCommand(
					m_db,
					BSON("findandmodify" << "astron.objects" <<
					     "query" << BSON(
						     "_id" << operation->m_doid) <<
					     "remove" << true),
					result);
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Unexpected error while deleting "
				               << operation->m_doid << ": " << e.what() << endl;
				operation->on_failure();
				return;
			}

			m_log->trace() << "handle_delete: got response: "
			               << result << endl;

			// If the findandmodify command failed, there wasn't anything there
			// to delete in the first place.
			if(!success || result["value"].isNull())
			{
				m_log->error() << "Tried to delete non-existent doid "
				               << operation->m_doid << endl;
				operation->on_failure();
				return;
			}

			free_doid(operation->m_doid);
			operation->on_complete();
		}

		void handle_get(DBOperation *operation)
		{
			BSONObj obj;
			try
			{
				obj = m_conn.findOne(m_obj_collection,
				                     BSON("_id" << operation->m_doid));
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Unexpected error occurred while trying to"
				                  " retrieve object with DOID "
				               << operation->m_doid << ": " << e.what() << endl;
				operation->on_failure();
				return;
			}

			if(obj.isEmpty())
			{
				m_log->warning() << "Got queried for non-existent object with DOID "
				                 << operation->m_doid << endl;
				operation->on_failure();
				return;
			}

			DBObjectSnapshot *snap = format_snapshot(operation->m_doid, obj);
			if(!snap || !operation->verify_class(snap->m_dclass))
			{
				operation->on_failure();
			}
			else
			{
				operation->on_complete(snap);
			}
		}

		void handle_modify(DBOperation *operation)
		{
			// First, we have to format our findandmodify.
			BSONObjBuilder sets;
			bool has_sets = false;
			BSONObjBuilder unsets;
			bool has_unsets = false;
			for(auto it  = operation->m_set_fields.begin();
			         it != operation->m_set_fields.end(); ++it)
			{
				stringstream fieldname;
				fieldname << "fields." << it->first->get_name();
				if(it->second.empty())
				{
					unsets << fieldname.str() << true;
					has_unsets = true;
				}
				else
				{
					BSONObjBuilder field;
					unpack_bson(it->first, it->second, field);
					sets << fieldname.str() << field.obj()[it->first->get_name()];
					has_sets = true;
				}
			}

			BSONObjBuilder updates_b;
			if(has_sets)
			{
				updates_b << "$set" << sets.obj();
			}
			if(has_unsets)
			{
				updates_b << "$unset" << unsets.obj();
			}
			BSONObj updates = updates_b.obj();

			// Also format any criteria for the change:
			BSONObjBuilder query_b;
			query_b << "_id" << operation->m_doid;
			for(auto it  = operation->m_criteria_fields.begin();
			         it != operation->m_criteria_fields.end(); ++it)
			{
				stringstream fieldname;
				fieldname << "fields." << it->first->get_name();
				if(it->second.empty())
				{
					query_b << fieldname.str() << BSON("$exists" << false);
				}
				else
				{
					BSONObjBuilder field;
					unpack_bson(it->first, it->second, field);
					query_b << fieldname.str() << field.obj()[it->first->get_name()];
				}
			}
			BSONObj query = query_b.obj();

			m_log->trace() << "Performing updates to " << operation->m_doid
			               << ": " << updates << endl;
			m_log->trace() << "Query is: " << query << endl;

			BSONObj result;
			bool success;
			try
			{
				success = m_conn.runCommand(
					m_db,
					BSON("findandmodify" << "astron.objects"
					     << "query" << query
					     << "update" << updates),
					result);
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Unexpected error while modifying "
				               << operation->m_doid << ": " << e.what() << endl;
				operation->on_failure();
				return;
			}

			m_log->trace() << "Update result: " << result << endl;

			BSONObj obj;
			if(!success || result["value"].isNull())
			{
				// Okay, something didn't work right. If we had criteria, let's
				// try to fetch the object without the criteria to see if it's a
				// criteria mismatch or a missing DOID.
				if(!operation->m_criteria_fields.empty())
				{
					try
					{
						obj = m_conn.findOne(m_obj_collection,
						                     BSON("_id" << operation->m_doid));
					}
					catch(bson::assertion &e)
					{
						m_log->error() << "Unexpected error while modifying "
						               << operation->m_doid << ": " << e.what() << endl;
						operation->on_failure();
						return;
					}
					if(!obj.isEmpty())
					{
						// There's the problem. Now we can send back a snapshot:
						DBObjectSnapshot *snap = format_snapshot(operation->m_doid, obj);
						if(snap && operation->verify_class(snap->m_dclass))
						{
							operation->on_criteria_mismatch(snap);
							return;
						}
						else
						{
							// Something else weird happened with our snapshot;
							// either the class wasn't recognized or it was the
							// wrong class. Either way, an error has been logged,
							// and we need to fail the operation.
							operation->on_failure();
							return;
						}
					}
				}

				// Nope, not that. We're missing the DOID.
				m_log->error() << "Attempted to modify unknown DOID: "
				               << operation->m_doid << endl;
				operation->on_failure();
				return;
			}

			// If we've gotten to this point: Hooray! The change has gone
			// through to the database.
			// Let's, however, double-check our changes. (Specifically, we should
			// run verify_class so that we know the frontend is happy with what
			// kind of object we just modified.)
			obj = result["value"].Obj();
			try
			{
				string dclass_name = obj["dclass"].String();
				const dclass::Class *dclass = g_dcf->get_class_by_name(dclass_name);
				if(!dclass)
				{
					m_log->error() << "Encountered unknown database object: "
					               << dclass_name << "(" << operation->m_doid << ")" << endl;
				}
				else if(operation->verify_class(dclass))
				{
					// Yep, it all checks out. Complete the operation:
					operation->on_complete();
					return;
				}
			}
			catch(bson::assertion &e) { }

			// If we've gotten here, something is seriously wrong. We've just
			// mucked with an object without knowing the consequences! What have
			// we done?! We've created an abomination in the database! Kill it!
			// Kill it with fire!

			// All we really can do to mitigate this is scream at the user (which
			// the above verification has already done by now) and revert the
			// object back to how it was when we found it.
			// NOTE: This DOES have the potential for data loss, because we're
			// wiping out any changes that conceivably could have happened
			// between the findandmodify and now. In dev environments, (which we
			// are probably in right now, if other components are making
			// outlandish requests like this) this shouldn't be a huge issue.
			m_log->trace() << "Reverting changes made to " << operation->m_doid << endl;
			try
			{
				m_conn.update(
						m_obj_collection,
						BSON("_id" << operation->m_doid),
						obj);
			}
			catch(bson::assertion &e)
			{
				// Wow, we REALLY fail at life.
				m_log->error() << "Could not revert corrupting changes to "
				               << operation->m_doid << ": " << e.what() << endl;
			}
			operation->on_failure();
		}

		// Get a DBObjectSnapshot from a MongoDB BSON object; returns NULL if failure.
		DBObjectSnapshot *format_snapshot(doid_t doid, const BSONObj &obj)
		{
			m_log->trace() << "Formatting database snapshot of " << doid << ": "
			               << obj << endl;
			try
			{
				string dclass_name = obj["dclass"].String();
				const dclass::Class *dclass = g_dcf->get_class_by_name(dclass_name);
				if(!dclass)
				{
					m_log->error() << "Encountered unknown database object: "
					               << dclass_name << "(" << doid << ")" << endl;
					return NULL;
				}

				BSONObj fields = obj["fields"].Obj();

				DBObjectSnapshot *snap = new DBObjectSnapshot();
				snap->m_dclass = dclass;
				for(auto it = fields.begin(); it.more(); ++it)
				{
					const char *name = (*it).fieldName();
					const dclass::Field *field = dclass->get_field_by_name(name);
					if(!field)
					{
						m_log->warning() << "Encountered unexpected field " << name
						                 << " while formatting " << dclass_name
						                 << "(" << doid << "); ignored." << endl;
						continue;
					}
					pack_bson(field, *it, snap->m_fields[field]);
				}

				return snap;
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Unexpected error while trying to format"
				                  " database snapshot for " << doid << ": "
				               << e.what() << endl;
				return NULL;
			}
		}

		// This function is used by handle_create to get a fresh DOID assignment.
		doid_t assign_doid()
		{
			try
			{
				if(!m_monotonic_exhausted)
				{
					doid_t doid = assign_doid_monotonic();
					if(doid == INVALID_DO_ID)
					{
						m_monotonic_exhausted = true;
					}
					else
					{
						return doid;
					}
				}

				// We've exhausted our supply of doids from the monotonic counter.
				// We must now resort to pulling things out of the free list:
				return assign_doid_reuse();
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Unexpected error occurred while trying to"
				                  " allocate a new DOID: " << e.what() << endl;
				return INVALID_DO_ID;
			}
		}

		doid_t assign_doid_monotonic()
		{
			BSONObj result;

			bool success = m_conn.runCommand(
				m_db,
				BSON("findandmodify" << "astron.globals" <<
				     "query" << BSON(
					     "_id" << "GLOBALS" <<
					     "doid.monotonic" << GTE << m_min_id <<
					     "doid.monotonic" << LTE << m_max_id
				     ) <<
				     "update" << BSON(
					     "$inc" << BSON("doid.monotonic" << 1)
				     )), result);

			// If the findandmodify command failed, the document either doesn't
			// exist, or we ran out of monotonic doids.
			if(!success || result["value"].isNull())
			{
				return INVALID_DO_ID;
			}

			m_log->trace() << "assign_doid_monotonic: got globals element: "
			               << result << endl;

			doid_t doid;
			const BSONElement &element = result["value"]["doid"]["monotonic"];
			if(sizeof(doid) == sizeof(long long))
			{
				doid = element.Long();
			}
			else if(sizeof(doid) == sizeof(int))
			{
				doid = element.Int();
			}
			return doid;
		}

		// This is used when the monotonic counter is exhausted:
		doid_t assign_doid_reuse()
		{
			BSONObj result;

			bool success = m_conn.runCommand(
				m_db,
				BSON("findandmodify" << "astron.globals" <<
				     "query" << BSON(
					     "_id" << "GLOBALS" <<
					     "doid.free.0" << BSON("$exists" << true)
				     ) <<
				     "update" << BSON(
					     "$pop" << BSON("doid.free" << -1)
				     )), result);

			// If the findandmodify command failed, the document either doesn't
			// exist, or we ran out of reusable doids.
			if(!success || result["value"].isNull())
			{
				m_log->error() << "Could not allocate a reused DOID!" << endl;
				return INVALID_DO_ID;
			}

			m_log->trace() << "assign_doid_reuse: got globals element: "
			               << result << endl;

			// Otherwise, use the first one:
			doid_t doid;
			const BSONElement &element = result["value"]["doid"]["free"];
			if(sizeof(doid) == sizeof(long long))
			{
				doid = element.Array()[0].Long();
			}
			else if(sizeof(doid) == sizeof(int))
			{
				doid = element.Array()[0].Int();
			}
			return doid;
		}

		// This returns a DOID to the free list:
		void free_doid(doid_t doid)
		{
			m_log->trace() << "Returning doid " << doid << " to the free pool..." << endl;

			try
			{
				m_conn.update(
					m_global_collection,
					BSON("_id" << "GLOBALS"),
					BSON("$push" << BSON("doid.free" << doid)));
			}
			catch(bson::assertion &e)
			{
				m_log->error() << "Could not return doid " << doid
				               << " to free pool: " << e.what() << endl;
			}
		}
};

DBBackendFactoryItem<MongoDatabase> mongodb_factory("mongodb");
