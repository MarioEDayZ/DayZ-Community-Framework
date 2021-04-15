class CF_ObjectManager_ObjectLink extends OLinkT
{
    int flags;
    int eventMask;
    bool hidden;
};

class CF_ObjectManager
{
    //!Single static instance. Do not create with new or spawn - use CF.ObjectManager for access instead.
    private void CF_ObjectManager();
    private void ~CF_ObjectManager();

    protected static const int HIDE_OBJECT_AXIS_OFFSET = 10000;

    protected static const ref map<Object, ref CF_ObjectManager_ObjectLink> m_RegisteredObjects = new map<Object, ref CF_ObjectManager_ObjectLink>();

    //!Storing as an array because GetHiddenMapObjects() would be O(n^3) in complexity
    protected static const ref array<CF_ObjectManager_ObjectLink> m_RegisteredObjectsArray = new array<CF_ObjectManager_ObjectLink>();

	/**
	 * @brief Registers obj to be exempt of being affected by the network bubble and the view distance.
	 *
	 * @param obj	Object pointer.
	 * @return bool	true of the object registered, false otherwise.
	 */
    static CF_ObjectManager_ObjectLink RegisterObject(Object object)
    {
        // Check to see if the object is already linked
        auto link = m_RegisteredObjects[object];
        if (link) return link;

        link = new CF_ObjectManager_ObjectLink(object);
        link.flags = object.GetFlags();
        link.eventMask = object.GetEventMask();
        link.hidden = false;

        m_RegisteredObjectsArray.Insert(link);
        m_RegisteredObjects.Set(object, link);

        return link;
    }

	/**
	 * @brief Unregisters obj so that it can be affected by the network bubble and the view distance.
     * @note Doesn't delete the link
	 *
	 * @param obj	Object pointer.
	 * @return bool	true of the object unregistered, false otherwise.
	 */
    static CF_ObjectManager_ObjectLink _UnregisterObject(Object object)
    {
        //Remove object from hidden collection
        auto link = m_RegisteredObjects[object];

        if (!link) return NULL; //Object not known as hidden

        m_RegisteredObjects.Remove(object);
        
        int idx = m_RegisteredObjectsArray.Find(link);
        if (idx != -1) m_RegisteredObjectsArray.Remove(idx);
        
        return link;
    }

	/**
	 * @brief Unregisters obj so that it can be affected by the network bubble and the view distance.
	 *
	 * @param obj	Object pointer.
	 * @return bool	true of the object unregistered, false otherwise.
	 */
    static void UnregisterObject(Object object)
    {
        //Remove object from hidden collection
        auto link = m_RegisteredObjects[object];

        if (!link) return; //Object not known as hidden

        m_RegisteredObjects.Remove(object);
        
        int idx = m_RegisteredObjectsArray.Find(link);
        if (idx != -1) m_RegisteredObjectsArray.Remove(idx);

        delete link;
    }

    /**
     * @brief Hides a static map object (Houses, Vegetation, etc.) visually and physically.
     * @code
     * Object hidden = CF.ObjectManager.HideMapObject(object);
     * @endcode
     *
     * @param object            Object to be hidden
     * @param updatePathGraph   Performs a path graph update after the object has been hidden. Enabled by default.
     * @return Object           instance of the object that was hidden, NULL in case of failure or invalid params.
     */
    static Object HideMapObject(Object object, bool updatePathGraph = true)
    {
        if (!IsMapObject(object)) return NULL; //Object is not a map object

        if (IsMapObjectHidden(object)) return NULL; //Object already hidden

        //Remember the original position for path graph updates
        auto originalPosition = object.GetPosition();

        //Register object in it's current state
        auto link = RegisterObject(object);

        link.hidden = true;

        object.ClearFlags( link.flags, true );
        object.ClearEventMask( link.eventMask );

        vector tm[4];
        object.GetTransform(tm);
        tm[3] = tm[3] - Vector(HIDE_OBJECT_AXIS_OFFSET, HIDE_OBJECT_AXIS_OFFSET, HIDE_OBJECT_AXIS_OFFSET);
        object.SetTransform(tm);
        object.Update();

        if (updatePathGraph && object.CanAffectPathgraph())
        {
            vector minMax[2];
            auto objectRadius = object.ClippingInfo(minMax);
            auto radiusVector = Vector(objectRadius, objectRadius, objectRadius);
            g_Game.UpdatePathgraphRegion(originalPosition - radiusVector, originalPosition + radiusVector);
        }

        return object;
    }

    /**
     * @brief Unhides an array of map objects
     * @code
     * array<Object> hidden = CF.ObjectManager.HideMapObjects(objects);
     * @endcode
     *
     * @param objects           Objects to be hidden
     * @param updatePathGraph   Performs a path graph update after the objects were hidden. Enabled by default.
     * @return array<Object>    Array of objects that were hidden.
     */
    static array<Object> HideMapObjects(array<Object> objects, bool updatePathGraph = true)
    {
        array<Object> hidden();

        foreach (auto object: objects)
        {
            /*
                Todo potential improvement:
                Determine if a single path graph update over the whole area is more effective that many small ones.
                Possible metrics:
                    - Object count to area ratio
                    - Max distance between objects
                    - Fixed radius lookup table
            */
            if (HideMapObject(object, updatePathGraph))
            {
                hidden.Insert(object);
            }
        }

        return hidden;
    }

    /**
     * @brief Hides static map objects at certain position and within a given radius
     * @code
     * array<Object> hidden = CF.ObjectManager.HideMapObjectsInRadius(position, 1000);
     * @endcode
     *
     * @param centerPosition    center coordinates for the hide area.
     * @param radius            radius of the hide area.
     * @param limitHeight       y-axis limit for the hide area (Sphere). Disabled by default.
     * @param updatePathGraph   Performs a path graph update after the objects were hidden. Enabled by default.
     * @return array<Object>    Array of objects that were hidden.
     */
    static array<Object> HideMapObjectsInRadius(vector centerPosition, float radius, bool limitHeight = false, bool updatePathGraph = true)
    {
        array<Object> objects();

        if (limitHeight)
        {
            GetGame().GetObjectsAtPosition3D(centerPosition, radius, objects, NULL);
        }
        else
        {
            GetGame().GetObjectsAtPosition(centerPosition, radius, objects, NULL);
        }

        array<Object> hidden();

        foreach (auto object: objects)
        {
            //Todo potential improvement -> s. HideMapObjects
            if (HideMapObject(object, updatePathGraph))
            {
                hidden.Insert(object);
            }
        }

        return hidden;
    }

    /**
     * @brief Unhides a hidden static map object (Houses, Vegetation, etc.).
     * @code
     * Object unhidden = CF.ObjectManager.UnhideMapObject(hiddenObject);
     * @endcode
     *
     * @param object            Object to be unhidden
     * @param updatePathGraph   Performs a path graph update after the object has been unhidden. Enabled by default.
     * @return Object           instance of the object that was unhidden, NULL in case of failure or invalid params.
     */
    static Object UnhideMapObject(Object object, bool updatePathGraph = true)
    {
        //Remove object from hidden collection
        auto link = _UnregisterObject(object);

        if (!link || !link.hidden) return NULL; //Object not known as hidden

        vector tm[4];
        object.GetTransform(tm);
        tm[3] = tm[3] + Vector(HIDE_OBJECT_AXIS_OFFSET, HIDE_OBJECT_AXIS_OFFSET, HIDE_OBJECT_AXIS_OFFSET);
        object.SetTransform(tm);

        object.SetFlags(link.flags, true);
        object.SetEventMask(link.eventMask);

        object.Update();

        if (updatePathGraph && object.CanAffectPathgraph())
        {
            vector minMax[2];
            auto objectPosition = object.GetPosition();
            auto objectRadius = object.ClippingInfo(minMax);
            auto radiusVector = Vector(objectRadius, objectRadius, objectRadius);
            g_Game.UpdatePathgraphRegion(objectPosition - radiusVector, objectPosition + radiusVector);
        }

        delete link;

        return object;
    }

    /**
     * @brief Unhides an array of map objects
     * @code
     * array<Object> unhidden = CF.ObjectManager.UnhideMapObjects(hiddenObjects);
     * @endcode
     *
     * @param objects           Objects to be unhidden
     * @param updatePathGraph   Performs a path graph update after the objects were unhidden. Enabled by default.
     * @return array<Object>    Array of objects that were unhidden.
     */
    static array<Object> UnhideMapObjects(array<Object> objects, bool updatePathGraph = true)
    {
        array<Object> unhidden();

        foreach (auto object: objects)
        {
            //Todo potential improvement -> s. HideMapObjects
            if (UnhideMapObject(object, updatePathGraph))
            {
                unhidden.Insert(object);
            }
        }

        return unhidden;
    }

    /**
     * @brief Unhides static map objects at certain position and within a given radius
     * @code
     * array<Object> unhidden = CF.ObjectManager.UnhideMapObjectsInRadius(position, 1000);
     * @endcode
     *
     * @param centerPosition    center coordinates for the unhide area.
     * @param radius            radius of the unhide area.
     * @param limitHeight       y-axis limit for the unhide area (Sphere). Disabled by default.
     * @param updatePathGraph   Performs a path graph update after the objects were unhidden. Enabled by default.
     * @return array<Object>    Array of objects that were unhidden.
     */
    static array<Object> UnhideMapObjectsInRadius(vector centerPosition, float radius, bool limitHeight = false, bool updatePathGraph = true)
    {
        array<Object> objects();

        if (limitHeight)
        {
            GetGame().GetObjectsAtPosition3D(centerPosition, radius, objects, NULL);
        }
        else
        {
            GetGame().GetObjectsAtPosition(centerPosition, radius, objects, NULL);
        }

        return UnhideMapObjects(objects, updatePathGraph);
    }

    /**
     * @brief Unhides all hidden map objects
     * @code
     * array<Object> unhidden = CF.ObjectManager.UnhideAllMapObjects();
     * @endcode
     *
     * @param updatePathGraph   Performs a path graph update after the objects were unhidden. Enabled by default.
     * @return array<Object>    Array of objects that were unhidden.
     */
	static array<Object> UnhideAllMapObjects(bool updatePathGraph = true)
	{
        array<Object> unhidden();

		for (int nObject = 0; nObject < m_RegisteredObjects.Count(); nObject++)
		{
            auto object = m_RegisteredObjects.GetKey(nObject);
            
            //Todo potential improvement -> s. HideMapObjects
            if (UnhideMapObject(object, updatePathGraph))
            {
                unhidden.Insert(object);
            }
		}

        return unhidden;
	}

    /**
     * @brief Returns all map objects that are currently hidden
     * @code
     * array<Object> objects = CF.ObjectManager.GetHiddenMapObjects();
     * @endcode
     *
     * @return array<Object> Array of hidden objects.
     */
	static array<Object> GetHiddenMapObjects()
	{
		ref array<Object> objects = new array<Object>;
		for (int i = 0; i < m_RegisteredObjectsArray.Count(); i++)
		{
            auto link = m_RegisteredObjectsArray[i];
            if (link.hidden) objects.Insert(link.Ptr());
		}
		return objects;
	}

    /**
     * @brief Returns all map objects that are currently hidden
     * @code
     * array<Object> objects = CF.ObjectManager.GetHiddenMapObjects();
     * @endcode
     *
     * @return array<Object> Array of hidden objects.
     */
	static array<Object> GetRegisteredObjects()
	{
		ref array<Object> objects = new array<Object>;
		for (int i = 0; i < m_RegisteredObjectsArray.Count(); i++)
		{
            auto link = m_RegisteredObjectsArray[i];
            objects.Insert(link.Ptr());
		}
		return objects;
	}

    /**
     * @brief Checks if a map object is currently hidden.
     * @code
     * bool isHiddenObject = CF.ObjectManager.IsMapObjectHidden(object);
     * @endcode
     *
     * @param object Object to check
     * @return bool true if currently hidden, false otherwise.
     */
    static bool IsMapObjectHidden(Object object)
    {
        CF_ObjectManager_ObjectLink link;
        if (!m_RegisteredObjects.Find(object, link)) return false;
        return link.hidden;
    }

    /**
     * @brief Checks if a map object is registered
     * @code
     * bool isRegistered = CF.ObjectManager.IsObjectRegistered(object);
     * @endcode
     *
     * @param object Object to check
     * @return bool true if currently registered, false otherwise.
     */
    static bool IsObjectRegistered(Object object)
    {
        return m_RegisteredObjects.Contains(object);
    }

    /**
     * @brief Checks if the given object is part of the baked map objects.
     * @code
     * bool isMapObject = CF.ObjectManager.IsMapObject(object);
     * @endcode
     *
     * @param object Object pointer
     * @return bool    true if it is a map object, false otherwise.
     */
    static bool IsMapObject(Object object)
    {
        if (!object) return false; //Null

        // Added via p3d in TB with no config.
        // Inherits from House in Cfg class -> Building, House, Wreck, Well, Tree, Bush, etc.
        return ((object.GetType() == string.Empty) && (object.Type() == Object)) || object.IsKindOf("House") || object.IsTree() || object.IsBush();
    }

    /**
     * @brief [Internal] CommunityFramework cleanup
     *
     * @return void
     */
    static void _Cleanup()
    {
        //Cleanup hidden object allocation
        m_RegisteredObjects.Clear();
        m_RegisteredObjectsArray.Clear();

        delete m_RegisteredObjects;
        delete m_RegisteredObjectsArray;
    }
};