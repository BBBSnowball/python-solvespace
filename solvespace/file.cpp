//-----------------------------------------------------------------------------
// Routines to write and read our .slvs file format.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

#define VERSION_STRING "���SolveSpaceREVa"

static int StrStartsWith(char *str, char *start) {
    return memcmp(str, start, strlen(start)) == 0;
}

//-----------------------------------------------------------------------------
// Clear and free all the dynamic memory associated with our currently-loaded
// sketch. This does not leave the program in an acceptable state (with the
// references created, and so on), so anyone calling this must fix that later.
//-----------------------------------------------------------------------------
void SolveSpace::ClearExisting(void) {
    UndoClearStack(&redo);
    UndoClearStack(&undo);

    Group *g;
    for(g = SK.group.First(); g; g = SK.group.NextAfter(g)) {
        g->Clear();
    }

    SK.constraint.Clear();
    SK.request.Clear();
    SK.group.Clear();
    SK.style.Clear();

    SK.entity.Clear();
    SK.param.Clear();
}

hGroup SolveSpace::CreateDefaultDrawingGroup(void) {
    Group g;
    ZERO(&g);

    // And an empty group, for the first stuff the user draws.
    g.visible = true;
    g.type = Group::DRAWING_WORKPLANE;
    g.subtype = Group::WORKPLANE_BY_POINT_ORTHO;
    g.predef.q = Quaternion::From(1, 0, 0, 0);
    hRequest hr = Request::HREQUEST_REFERENCE_XY;
    g.predef.origin = hr.entity(1);
    g.name.strcpy("sketch-in-plane");
    SK.group.AddAndAssignId(&g);
    SK.GetGroup(g.h)->activeWorkplane = g.h.entity(0);
    return g.h;
}

void SolveSpace::NewFile(void) {
    ClearExisting();

    // Our initial group, that contains the references.
    Group g;
    memset(&g, 0, sizeof(g));
    g.visible = true;
    g.name.strcpy("#references");
    g.type = Group::DRAWING_3D;
    g.h = Group::HGROUP_REFERENCES;
    SK.group.Add(&g);

    // Let's create three two-d coordinate systems, for the coordinate
    // planes; these are our references, present in every sketch.
    Request r;
    ZERO(&r);
    r.type = Request::WORKPLANE;
    r.group = Group::HGROUP_REFERENCES;
    r.workplane = Entity::FREE_IN_3D;

    r.h = Request::HREQUEST_REFERENCE_XY;
    SK.request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_YZ;
    SK.request.Add(&r);

    r.h = Request::HREQUEST_REFERENCE_ZX;
    SK.request.Add(&r);

    CreateDefaultDrawingGroup();
}

const SolveSpace::SaveTable SolveSpace::SAVED[] = {
    { 'g',  "Group.h.v",                'x',    &(SS.sv.g.h.v)                },
    { 'g',  "Group.type",               'd',    &(SS.sv.g.type)               },
    { 'g',  "Group.order",              'd',    &(SS.sv.g.order)              },
    { 'g',  "Group.name",               'N',    &(SS.sv.g.name)               },
    { 'g',  "Group.activeWorkplane.v",  'x',    &(SS.sv.g.activeWorkplane.v)  },
    { 'g',  "Group.opA.v",              'x',    &(SS.sv.g.opA.v)              },
    { 'g',  "Group.opB.v",              'x',    &(SS.sv.g.opB.v)              },
    { 'g',  "Group.valA",               'f',    &(SS.sv.g.valA)               },
    { 'g',  "Group.valB",               'f',    &(SS.sv.g.valB)               },
    { 'g',  "Group.valC",               'f',    &(SS.sv.g.valB)               },
    { 'g',  "Group.color",              'x',    &(SS.sv.g.color)              },
    { 'g',  "Group.subtype",            'd',    &(SS.sv.g.subtype)            },
    { 'g',  "Group.skipFirst",          'b',    &(SS.sv.g.skipFirst)          },
    { 'g',  "Group.meshCombine",        'd',    &(SS.sv.g.meshCombine)        },
    { 'g',  "Group.forceToMesh",        'd',    &(SS.sv.g.forceToMesh)        },
    { 'g',  "Group.predef.q.w",         'f',    &(SS.sv.g.predef.q.w)         },
    { 'g',  "Group.predef.q.vx",        'f',    &(SS.sv.g.predef.q.vx)        },
    { 'g',  "Group.predef.q.vy",        'f',    &(SS.sv.g.predef.q.vy)        },
    { 'g',  "Group.predef.q.vz",        'f',    &(SS.sv.g.predef.q.vz)        },
    { 'g',  "Group.predef.origin.v",    'x',    &(SS.sv.g.predef.origin.v)    },
    { 'g',  "Group.predef.entityB.v",   'x',    &(SS.sv.g.predef.entityB.v)   },
    { 'g',  "Group.predef.entityC.v",   'x',    &(SS.sv.g.predef.entityC.v)   },
    { 'g',  "Group.predef.swapUV",      'b',    &(SS.sv.g.predef.swapUV)      },
    { 'g',  "Group.predef.negateU",     'b',    &(SS.sv.g.predef.negateU)     },
    { 'g',  "Group.predef.negateV",     'b',    &(SS.sv.g.predef.negateV)     },
    { 'g',  "Group.visible",            'b',    &(SS.sv.g.visible)            },
    { 'g',  "Group.suppress",           'b',    &(SS.sv.g.suppress)           },
    { 'g',  "Group.relaxConstraints",   'b',    &(SS.sv.g.relaxConstraints)   },
    { 'g',  "Group.allDimsReference",   'b',    &(SS.sv.g.allDimsReference)   },
    { 'g',  "Group.scale",              'f',    &(SS.sv.g.scale)              },
    { 'g',  "Group.remap",              'M',    &(SS.sv.g.remap)              },
    { 'g',  "Group.impFile",            'P',    &(SS.sv.g.impFile)            },
    { 'g',  "Group.impFileRel",         'P',    &(SS.sv.g.impFileRel)         },

    { 'p',  "Param.h.v.",               'x',    &(SS.sv.p.h.v)                },
    { 'p',  "Param.val",                'f',    &(SS.sv.p.val)                },

    { 'r',  "Request.h.v",              'x',    &(SS.sv.r.h.v)                },
    { 'r',  "Request.type",             'd',    &(SS.sv.r.type)               },
    { 'r',  "Request.extraPoints",      'd',    &(SS.sv.r.extraPoints)        },
    { 'r',  "Request.workplane.v",      'x',    &(SS.sv.r.workplane.v)        },
    { 'r',  "Request.group.v",          'x',    &(SS.sv.r.group.v)            },
    { 'r',  "Request.construction",     'b',    &(SS.sv.r.construction)       },
    { 'r',  "Request.style",            'x',    &(SS.sv.r.style)              },
    { 'r',  "Request.str",              'N',    &(SS.sv.r.str)                },
    { 'r',  "Request.font",             'N',    &(SS.sv.r.font)               },

    { 'e',  "Entity.h.v",               'x',    &(SS.sv.e.h.v)                },
    { 'e',  "Entity.type",              'd',    &(SS.sv.e.type)               },
    { 'e',  "Entity.construction",      'b',    &(SS.sv.e.construction)       },
    { 'e',  "Entity.style",             'x',    &(SS.sv.e.style)              },
    { 'e',  "Entity.str",               'N',    &(SS.sv.e.str)                },
    { 'e',  "Entity.font",              'N',    &(SS.sv.e.font)               },
    { 'e',  "Entity.point[0].v",        'x',    &(SS.sv.e.point[0].v)         },
    { 'e',  "Entity.point[1].v",        'x',    &(SS.sv.e.point[1].v)         },
    { 'e',  "Entity.point[2].v",        'x',    &(SS.sv.e.point[2].v)         },
    { 'e',  "Entity.point[3].v",        'x',    &(SS.sv.e.point[3].v)         },
    { 'e',  "Entity.point[4].v",        'x',    &(SS.sv.e.point[4].v)         },
    { 'e',  "Entity.point[5].v",        'x',    &(SS.sv.e.point[5].v)         },
    { 'e',  "Entity.point[6].v",        'x',    &(SS.sv.e.point[6].v)         },
    { 'e',  "Entity.point[7].v",        'x',    &(SS.sv.e.point[7].v)         },
    { 'e',  "Entity.point[8].v",        'x',    &(SS.sv.e.point[8].v)         },
    { 'e',  "Entity.point[9].v",        'x',    &(SS.sv.e.point[9].v)         },
    { 'e',  "Entity.point[10].v",       'x',    &(SS.sv.e.point[10].v)        },
    { 'e',  "Entity.point[11].v",       'x',    &(SS.sv.e.point[11].v)        },
    { 'e',  "Entity.extraPoints",       'd',    &(SS.sv.e.extraPoints)        },
    { 'e',  "Entity.normal.v",          'x',    &(SS.sv.e.normal.v)           },
    { 'e',  "Entity.distance.v",        'x',    &(SS.sv.e.distance.v)         },
    { 'e',  "Entity.workplane.v",       'x',    &(SS.sv.e.workplane.v)        },
    { 'e',  "Entity.actPoint.x",        'f',    &(SS.sv.e.actPoint.x)         },
    { 'e',  "Entity.actPoint.y",        'f',    &(SS.sv.e.actPoint.y)         },
    { 'e',  "Entity.actPoint.z",        'f',    &(SS.sv.e.actPoint.z)         },
    { 'e',  "Entity.actNormal.w",       'f',    &(SS.sv.e.actNormal.w)        },
    { 'e',  "Entity.actNormal.vx",      'f',    &(SS.sv.e.actNormal.vx)       },
    { 'e',  "Entity.actNormal.vy",      'f',    &(SS.sv.e.actNormal.vy)       },
    { 'e',  "Entity.actNormal.vz",      'f',    &(SS.sv.e.actNormal.vz)       },
    { 'e',  "Entity.actDistance",       'f',    &(SS.sv.e.actDistance)        },
    { 'e',  "Entity.actVisible",        'b',    &(SS.sv.e.actVisible),        },


    { 'c',  "Constraint.h.v",           'x',    &(SS.sv.c.h.v)                },
    { 'c',  "Constraint.type",          'd',    &(SS.sv.c.type)               },
    { 'c',  "Constraint.group.v",       'x',    &(SS.sv.c.group.v)            },
    { 'c',  "Constraint.workplane.v",   'x',    &(SS.sv.c.workplane.v)        },
    { 'c',  "Constraint.valA",          'f',    &(SS.sv.c.valA)               },
    { 'c',  "Constraint.ptA.v",         'x',    &(SS.sv.c.ptA.v)              },
    { 'c',  "Constraint.ptB.v",         'x',    &(SS.sv.c.ptB.v)              },
    { 'c',  "Constraint.entityA.v",     'x',    &(SS.sv.c.entityA.v)          },
    { 'c',  "Constraint.entityB.v",     'x',    &(SS.sv.c.entityB.v)          },
    { 'c',  "Constraint.entityC.v",     'x',    &(SS.sv.c.entityC.v)          },
    { 'c',  "Constraint.entityD.v",     'x',    &(SS.sv.c.entityD.v)          },
    { 'c',  "Constraint.other",         'b',    &(SS.sv.c.other)              },
    { 'c',  "Constraint.other2",        'b',    &(SS.sv.c.other2)             },
    { 'c',  "Constraint.reference",     'b',    &(SS.sv.c.reference)          },
    { 'c',  "Constraint.comment",       'N',    &(SS.sv.c.comment)            },
    { 'c',  "Constraint.disp.offset.x", 'f',    &(SS.sv.c.disp.offset.x)      },
    { 'c',  "Constraint.disp.offset.y", 'f',    &(SS.sv.c.disp.offset.y)      },
    { 'c',  "Constraint.disp.offset.z", 'f',    &(SS.sv.c.disp.offset.z)      },
    { 'c',  "Constraint.disp.style",    'x',    &(SS.sv.c.disp.style)         },

    { 's',  "Style.h.v",                'x',    &(SS.sv.s.h.v)                },
    { 's',  "Style.name",               'N',    &(SS.sv.s.name)               },
    { 's',  "Style.width",              'f',    &(SS.sv.s.width)              },
    { 's',  "Style.widthAs",            'd',    &(SS.sv.s.widthAs)            },
    { 's',  "Style.textHeight",         'f',    &(SS.sv.s.textHeight)         },
    { 's',  "Style.textHeightAs",       'd',    &(SS.sv.s.textHeightAs)       },
    { 's',  "Style.textAngle",          'f',    &(SS.sv.s.textAngle)          },
    { 's',  "Style.textOrigin",         'x',    &(SS.sv.s.textOrigin)         },
    { 's',  "Style.color",              'x',    &(SS.sv.s.color)              },
    { 's',  "Style.fillColor",          'x',    &(SS.sv.s.fillColor)          },
    { 's',  "Style.filled",             'b',    &(SS.sv.s.filled)             },
    { 's',  "Style.visible",            'b',    &(SS.sv.s.visible)            },
    { 's',  "Style.exportable",         'b',    &(SS.sv.s.exportable)         },

    { 0, NULL, NULL, NULL     },
};

void SolveSpace::SaveUsingTable(int type) {
    int i;
    for(i = 0; SAVED[i].type != 0; i++) {
        if(SAVED[i].type != type) continue;

        int fmt = SAVED[i].fmt;
        void *p = SAVED[i].ptr;
        // Any items that aren't specified are assumed to be zero
        if(fmt == 'd' && *((int *)p)    == 0)   continue;
        if(fmt == 'x' && *((DWORD *)p)  == 0)   continue;
        if(fmt == 'f' && *((double *)p) == 0.0) continue;
        if(fmt == 'N' && strlen(((NameStr *)p)->str) == 0) continue;

        fprintf(fh, "%s=", SAVED[i].desc);
        switch(fmt) {
            case 'd': fprintf(fh, "%d", *((int *)p)); break;
            case 'b': fprintf(fh, "%d", *((bool *)p) ? 1 : 0); break;
            case 'x': fprintf(fh, "%08x", *((DWORD *)p)); break;
            case 'f': fprintf(fh, "%.20f", *((double *)p)); break;
            case 'N': fprintf(fh, "%s", ((NameStr *)p)->str); break;
            case 'P': fprintf(fh, "%s", (char *)p); break;

            case 'M': {
                int j;
                fprintf(fh, "{\n");
                IdList<EntityMap,EntityId> *m = (IdList<EntityMap,EntityId> *)p;
                for(j = 0; j < m->n; j++) {
                    EntityMap *em = &(m->elem[j]);
                    fprintf(fh, "    %d %08x %d\n", 
                            em->h.v, em->input.v, em->copyNumber);
                }
                fprintf(fh, "}");
                break;
            }

            default: oops();
        }
        fprintf(fh, "\n");
    }
}

bool SolveSpace::SaveToFile(char *filename) {
    // Make sure all the entities are regenerated up to date, since they
    // will be exported.
    SS.GenerateAll(0, INT_MAX);

    fh = fopen(filename, "wb");
    if(!fh) {   
        Error("Couldn't write to file '%s'", filename);
        return false;
    }

    fprintf(fh, "%s\n\n\n", VERSION_STRING);

    int i, j;
    for(i = 0; i < SK.group.n; i++) {
        sv.g = SK.group.elem[i];
        SaveUsingTable('g');
        fprintf(fh, "AddGroup\n\n");
    }

    for(i = 0; i < SK.param.n; i++) {
        sv.p = SK.param.elem[i];
        SaveUsingTable('p');
        fprintf(fh, "AddParam\n\n");
    }

    for(i = 0; i < SK.request.n; i++) {
        sv.r = SK.request.elem[i];
        SaveUsingTable('r');
        fprintf(fh, "AddRequest\n\n");
    }

    for(i = 0; i < SK.entity.n; i++) {
        (SK.entity.elem[i]).CalculateNumerical(true);
        sv.e = SK.entity.elem[i];
        SaveUsingTable('e');
        fprintf(fh, "AddEntity\n\n");
    }

    for(i = 0; i < SK.constraint.n; i++) {
        sv.c = SK.constraint.elem[i];
        SaveUsingTable('c');
        fprintf(fh, "AddConstraint\n\n");
    }

    for(i = 0; i < SK.style.n; i++) {
        sv.s = SK.style.elem[i];
        if(sv.s.h.v >= Style::FIRST_CUSTOM) {
            SaveUsingTable('s');
            fprintf(fh, "AddStyle\n\n");
        }
    }

    // A group will have either a mesh or a shell, but not both; but the code
    // to print either of those just does nothing if the mesh/shell is empty.

    SMesh *m = &(SK.group.elem[SK.group.n-1].runningMesh);
    for(i = 0; i < m->l.n; i++) {
        STriangle *tr = &(m->l.elem[i]);
        fprintf(fh, "Triangle %08x %08x  "
                "%.20f %.20f %.20f  %.20f %.20f %.20f  %.20f %.20f %.20f\n",
            tr->meta.face, tr->meta.color,
            CO(tr->a), CO(tr->b), CO(tr->c));
    }

    SShell *s = &(SK.group.elem[SK.group.n-1].runningShell);
    SSurface *srf;
    for(srf = s->surface.First(); srf; srf = s->surface.NextAfter(srf)) {
        fprintf(fh, "Surface %08x %08x %08x %d %d\n",
                        srf->h.v, srf->color, srf->face, srf->degm, srf->degn);
        for(i = 0; i <= srf->degm; i++) {
            for(j = 0; j <= srf->degn; j++) {
                fprintf(fh, "SCtrl %d %d %.20f %.20f %.20f Weight %20.20f\n",
                    i, j, CO(srf->ctrl[i][j]), srf->weight[i][j]);
            }
        }
        
        STrimBy *stb;
        for(stb = srf->trim.First(); stb; stb = srf->trim.NextAfter(stb)) {
            fprintf(fh, "TrimBy %08x %d %.20f %.20f %.20f  %.20f %.20f %.20f\n",
                stb->curve.v, stb->backwards ? 1 : 0,
                CO(stb->start), CO(stb->finish));
        }

        fprintf(fh, "AddSurface\n");
    }
    SCurve *sc;
    for(sc = s->curve.First(); sc; sc = s->curve.NextAfter(sc)) {
        fprintf(fh, "Curve %08x %d %d %08x %08x\n",
            sc->h.v,
            sc->isExact ? 1 : 0, sc->exact.deg,
            sc->surfA.v, sc->surfB.v);

        if(sc->isExact) {
            for(i = 0; i <= sc->exact.deg; i++) {
                fprintf(fh, "CCtrl %d %.20f %.20f %.20f Weight %.20f\n",
                    i, CO(sc->exact.ctrl[i]), sc->exact.weight[i]);
            }
        }
        SCurvePt *scpt;
        for(scpt = sc->pts.First(); scpt; scpt = sc->pts.NextAfter(scpt)) {
            fprintf(fh, "CurvePt %d %.20f %.20f %.20f\n",
                scpt->vertex ? 1 : 0, CO(scpt->p));
        }

        fprintf(fh, "AddCurve\n");
    }

    fclose(fh);

    return true;
}

void SolveSpace::LoadUsingTable(char *key, char *val) {
    int i;
    for(i = 0; SAVED[i].type != 0; i++) {
        if(strcmp(SAVED[i].desc, key)==0) {
            void *p = SAVED[i].ptr;
            switch(SAVED[i].fmt) {
                case 'd': *((int *)p) = atoi(val); break;
                case 'b': *((bool *)p) = (atoi(val) != 0); break;
                case 'x': sscanf(val, "%x", (DWORD *)p); break;
                case 'f': *((double *)p) = atof(val); break;
                case 'N': ((NameStr *)p)->strcpy(val); break;

                case 'P':
                    if(strlen(val)+1 < MAX_PATH) strcpy((char *)p, val);
                    break;

                case 'M': {
                    IdList<EntityMap,EntityId> *m =
                                (IdList<EntityMap,EntityId> *)p;
                    // Don't clear this list! When the group gets added, it
                    // makes a shallow copy, so that would result in us
                    // freeing memory that we want to keep around. Just
                    // zero it out so that new memory is allocated.
                    memset(m, 0, sizeof(*m));
                    for(;;) {
                        EntityMap em;
                        char line2[1024];
                        fgets(line2, sizeof(line2), fh);
                        if(sscanf(line2, "%d %x %d", &(em.h.v), &(em.input.v),
                                                     &(em.copyNumber)) == 3)
                        {
                            m->Add(&em);
                        } else {
                            break;
                        }
                    }
                    break;
                }

                default: oops();
            }
            break;
        }
    }
    if(SAVED[i].type == 0) {
        fileLoadError = true;
    }
}

bool SolveSpace::LoadFromFile(char *filename) {
    allConsistent = false;
    fileLoadError = false;

    fh = fopen(filename, "rb");
    if(!fh) {   
        Error("Couldn't read from file '%s'", filename);
        return false;
    }

    ClearExisting();

    memset(&sv, 0, sizeof(sv));
    sv.g.scale = 1; // default is 1, not 0; so legacy files need this

    char line[1024];
    while(fgets(line, sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';

        if(*line == '\0') continue;
       
        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(key, val);
        } else if(strcmp(line, "AddGroup")==0) {
            SK.group.Add(&(sv.g));
            ZERO(&(sv.g));
            sv.g.scale = 1; // default is 1, not 0; so legacy files need this
        } else if(strcmp(line, "AddParam")==0) {
            // params are regenerated, but we want to preload the values
            // for initial guesses
            SK.param.Add(&(sv.p));
            ZERO(&(sv.p));
        } else if(strcmp(line, "AddEntity")==0) {
            // entities are regenerated
        } else if(strcmp(line, "AddRequest")==0) {
            SK.request.Add(&(sv.r));
            ZERO(&(sv.r));
        } else if(strcmp(line, "AddConstraint")==0) {
            SK.constraint.Add(&(sv.c));
            ZERO(&(sv.c));
        } else if(strcmp(line, "AddStyle")==0) {
            SK.style.Add(&(sv.s));
            ZERO(&(sv.s));
        } else if(strcmp(line, VERSION_STRING)==0) {
            // do nothing, version string
        } else if(StrStartsWith(line, "Triangle ")      ||
                  StrStartsWith(line, "Surface ")       ||
                  StrStartsWith(line, "SCtrl ")         ||
                  StrStartsWith(line, "TrimBy ")        ||
                  StrStartsWith(line, "Curve ")         ||
                  StrStartsWith(line, "CCtrl ")         ||
                  StrStartsWith(line, "CurvePt ")       ||
                  strcmp(line, "AddSurface")==0         ||
                  strcmp(line, "AddCurve")==0)
        {
            // ignore the mesh or shell, since we regenerate that
        } else {
            fileLoadError = true;
        }
    }

    fclose(fh);

    if(fileLoadError) {
        Error("Unrecognized data in file. This file may be corrupt, or "
              "from a new version of the program.");
        // At least leave the program in a non-crashing state.
        if(SK.group.n == 0) {
            NewFile();
        }
    }

    return true;
}

bool SolveSpace::LoadEntitiesFromFile(char *file, EntityList *le,
                                      SMesh *m, SShell *sh)
{
    SSurface srf;
    ZERO(&srf);
    SCurve crv;
    ZERO(&crv);

    fh = fopen(file, "rb");
    if(!fh) return false;

    le->Clear();
    memset(&sv, 0, sizeof(sv));

    char line[1024];
    while(fgets(line, sizeof(line), fh)) {
        char *s = strchr(line, '\n');
        if(s) *s = '\0';
        // We should never get files with \r characters in them, but mailers
        // will sometimes mangle attachments.
        s = strchr(line, '\r');
        if(s) *s = '\0';

        if(*line == '\0') continue;
       
        char *e = strchr(line, '=');
        if(e) {
            *e = '\0';
            char *key = line, *val = e+1;
            LoadUsingTable(key, val);
        } else if(strcmp(line, "AddGroup")==0) {
            // Don't leak memory; these get allocated whether we want them
            // or not.
            sv.g.remap.Clear();
        } else if(strcmp(line, "AddParam")==0) {

        } else if(strcmp(line, "AddEntity")==0) {
            le->Add(&(sv.e));
            memset(&(sv.e), 0, sizeof(sv.e));
        } else if(strcmp(line, "AddRequest")==0) {

        } else if(strcmp(line, "AddConstraint")==0) {

        } else if(strcmp(line, "AddStyle")==0) {

        } else if(strcmp(line, VERSION_STRING)==0) {

        } else if(StrStartsWith(line, "Triangle ")) {
            STriangle tr; ZERO(&tr);
            if(sscanf(line, "Triangle %x %x  "
                             "%lf %lf %lf  %lf %lf %lf  %lf %lf %lf",
                &(tr.meta.face), &(tr.meta.color),
                &(tr.a.x), &(tr.a.y), &(tr.a.z), 
                &(tr.b.x), &(tr.b.y), &(tr.b.z), 
                &(tr.c.x), &(tr.c.y), &(tr.c.z)) != 11)
            {
                oops();
            }
            m->AddTriangle(&tr);
        } else if(StrStartsWith(line, "Surface ")) {
            if(sscanf(line, "Surface %x %x %x %d %d",
                &(srf.h.v), &(srf.color), &(srf.face),
                &(srf.degm), &(srf.degn)) != 5)
            {
                oops();
            }
        } else if(StrStartsWith(line, "SCtrl ")) {
            int i, j;
            Vector c;
            double w;
            if(sscanf(line, "SCtrl %d %d %lf %lf %lf Weight %lf",
                                &i, &j, &(c.x), &(c.y), &(c.z), &w) != 6)
            {
                oops();
            }
            srf.ctrl[i][j] = c;
            srf.weight[i][j] = w;
        } else if(StrStartsWith(line, "TrimBy ")) {
            STrimBy stb;
            ZERO(&stb);
            int backwards;
            if(sscanf(line, "TrimBy %x %d  %lf %lf %lf  %lf %lf %lf",
                &(stb.curve.v), &backwards,
                &(stb.start.x), &(stb.start.y), &(stb.start.z),
                &(stb.finish.x), &(stb.finish.y), &(stb.finish.z)) != 8)
            {
                oops();
            }
            stb.backwards = (backwards != 0);
            srf.trim.Add(&stb);
        } else if(strcmp(line, "AddSurface")==0) {
            sh->surface.Add(&srf);
            ZERO(&srf);
        } else if(StrStartsWith(line, "Curve ")) {
            int isExact;
            if(sscanf(line, "Curve %x %d %d %x %x",
                &(crv.h.v),
                &(isExact),
                &(crv.exact.deg),
                &(crv.surfA.v), &(crv.surfB.v)) != 5)
            {
                oops();
            }
            crv.isExact = (isExact != 0);
        } else if(StrStartsWith(line, "CCtrl ")) {
            int i;
            Vector c;
            double w;
            if(sscanf(line, "CCtrl %d %lf %lf %lf Weight %lf",
                                &i, &(c.x), &(c.y), &(c.z), &w) != 5)
            {
                oops();
            }
            crv.exact.ctrl[i] = c;
            crv.exact.weight[i] = w;
        } else if(StrStartsWith(line, "CurvePt ")) {
            SCurvePt scpt;
            int vertex;
            if(sscanf(line, "CurvePt %d %lf %lf %lf",
                &vertex,
                &(scpt.p.x), &(scpt.p.y), &(scpt.p.z)) != 4)
            {
                oops();
            }
            scpt.vertex = (vertex != 0);
            crv.pts.Add(&scpt);
        } else if(strcmp(line, "AddCurve")==0) {
            sh->curve.Add(&crv);
            ZERO(&crv);
        } else {
            oops();
        }
    }

    fclose(fh);
    return true;
}

void SolveSpace::ReloadAllImported(void) {
    allConsistent = false;

    int i;
    for(i = 0; i < SK.group.n; i++) {
        Group *g = &(SK.group.elem[i]);
        if(g->type != Group::IMPORTED) continue;

        g->impEntity.Clear();
        g->impMesh.Clear();
        g->impShell.Clear();

        FILE *test = fopen(g->impFile, "rb");
        if(test) {
            fclose(test); // okay, exists
        } else {
            // It doesn't exist. Perhaps the entire tree has moved, and we
            // can use the relative filename to get us back.
            if(SS.saveFile[0]) {
                char fromRel[MAX_PATH];
                strcpy(fromRel, g->impFileRel);
                MakePathAbsolute(SS.saveFile, fromRel);
                test = fopen(fromRel, "rb");
                if(test) {
                    fclose(test);
                    // It worked, this is our new absolute path
                    strcpy(g->impFile, fromRel);
                }
            }
        }

        if(LoadEntitiesFromFile(g->impFile,
                        &(g->impEntity), &(g->impMesh), &(g->impShell)))
        {
            if(SS.saveFile[0]) {
                // Record the imported file's name relative to our filename;
                // if the entire tree moves, then everything will still work
                strcpy(g->impFileRel, g->impFile);
                MakePathRelative(SS.saveFile, g->impFileRel);
            } else {
                // We're not yet saved, so can't make it absolute
                strcpy(g->impFileRel, g->impFile);
            }
        } else {
            Error("Failed to load imported file '%s'", g->impFile);
        }
    }
}

