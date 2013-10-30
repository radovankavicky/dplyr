#ifndef dplyr_VectorVisitor_Impl_H
#define dplyr_VectorVisitor_Impl_H

namespace dplyr {

    template <typename Container>
    inline int output_size( const Container& container){
        return container.size() ;
    }
    
    template <>
    inline int output_size<LogicalVector>( const LogicalVector& container){
        return std::count( container.begin(), container.end(), 1 ) ;
    }
    
    /** 
     * Implementations 
     */
    template <int RTYPE>
    class VectorVisitorImpl : public VectorVisitor, public comparisons<RTYPE> {
    public:
        typedef comparisons<RTYPE> compare ;
        typedef Rcpp::Vector<RTYPE> VECTOR ;
        
        /** 
         * The type of data : int, double, SEXP, Rcomplex
         */
        typedef typename Rcpp::traits::storage_type<RTYPE>::type STORAGE ;
        
        /**
         * Hasher for that type of data
         */
        typedef boost::hash<STORAGE> hasher ;
        
        VectorVisitorImpl( const VECTOR& vec_ ) : vec(vec_) {}
            
        /** 
         * implementations
         */
        size_t hash(int i){ 
            return hash_fun( vec[i] ) ;
        } 
        inline bool equal(int i, int j){ 
            return compare::is_equal( vec[i], vec[j] ) ;
        }
        
        inline bool less(int i, int j){ 
            return compare::is_less( vec[i], vec[j] ) ;
        }
        
        inline bool greater(int i, int j){ 
            return compare::is_greater( vec[i], vec[j] ) ;
        }
        
        inline SEXP subset( const Rcpp::IntegerVector& index){
            return subset_int_index( index) ;    
        }
        inline SEXP subset( const std::vector<int>& index){
            return subset_int_index( index) ;    
        }
        
        inline SEXP subset( const ChunkIndexMap& map ){
            int n = output_size(map) ;
            VECTOR out = Rcpp::no_init(n) ;
            ChunkIndexMap::const_iterator it = map.begin(); 
            for( int i=0; i<n; i++, ++it)
                out[i] = vec[ it->first ] ;
            return out ;
        }
        
        inline SEXP subset( const Rcpp::LogicalVector& index ){
            int n = output_size(index) ;
            VECTOR out = Rcpp::no_init(n) ;
            for( int i=0, k=0; k<n; k++, i++ ) {
                while( ! index[i] ) i++; 
                out[k] = vec[i] ;
            }
            return out ;
        }
        
        
    protected: 
        VECTOR vec ;
        hasher hash_fun ;
        
        template <typename Container>
        inline SEXP subset_int_index( const Container& index ) {
            int n = output_size(index) ;
            VECTOR out = Rcpp::no_init(n) ;
            // TODO: find a way to mark that we don't need the NA handling
            for( int i=0; i<n; i++) 
                out[i] = (index[i] < 0) ? VECTOR::get_na() : vec[ index[i] ] ;
            return out ;
        }
        
    } ;
    
    template <typename Class, typename VisitorImpl> 
    class PromoteClassVisitor : public VisitorImpl {
    public:
        typedef typename VisitorImpl::VECTOR VECTOR ;
        PromoteClassVisitor( const VECTOR& vec ) : VisitorImpl(vec){}
        
        inline SEXP subset( const Rcpp::IntegerVector& index){
            return promote( VisitorImpl::subset( index ) );
        }
        
        inline SEXP subset( const std::vector<int>& index){
            return promote( VisitorImpl::subset( index ) ) ;    
        }
        
        inline SEXP subset( const ChunkIndexMap& map ){
            return promote( VisitorImpl::subset( map ) ) ;
        }
        
        inline SEXP subset( const Rcpp::LogicalVector& index ){
            return promote( VisitorImpl::subset( index ) ) ;
        }
    private:
        inline SEXP promote( NumericVector x){
            x.attr( "class" ) = VisitorImpl::vec.attr( "class" ) ;
            return x ;
        }
    } ;
    
    class FactorVisitor : public VectorVisitorImpl<INTSXP> {
    public:    
        typedef VectorVisitorImpl<INTSXP> Parent ;
        typedef comparisons<STRSXP> string_compare ;
        
        FactorVisitor( const IntegerVector& vec ) : 
            Parent(vec), levels( vec.attr( "levels" ) ), levels_ptr(Rcpp::internal::r_vector_start<STRSXP>(levels) ) {}
        
        inline bool equal(int i, int j){ 
            return string_compare::is_equal( levels_ptr[vec[i]], levels_ptr[vec[j]] ) ;
        }
        
        inline bool less(int i, int j){ 
            return compare::is_less( levels_ptr[vec[i]], levels_ptr[vec[j]] ) ;
        }
        
        inline bool greater(int i, int j){ 
            return compare::is_greater( levels_ptr[vec[i]], levels_ptr[vec[j]] ) ;
        }
            
        inline SEXP subset( const Rcpp::IntegerVector& index){
            return promote( Parent::subset( index ) );
        }
        
        inline SEXP subset( const std::vector<int>& index){
            return promote( Parent::subset( index ) ) ;    
        }
        
        inline SEXP subset( const ChunkIndexMap& map ){
            return promote( Parent::subset( map ) ) ;
        }
        
        inline SEXP subset( const Rcpp::LogicalVector& index ){
            return promote( Parent::subset( index ) ) ;
        }
        
    private:
    
        inline SEXP promote( IntegerVector x){
            x.attr( "class" ) = vec.attr( "class" );
            x.attr( "levels" ) = levels ;
        }
        
        CharacterVector levels ;
        SEXP* levels_ptr ;
    } ;
    
    #define PROMOTE_VISITOR(__CLASS__)                                                   \
    class __CLASS__ : public PromoteClassVisitor<__CLASS__, VectorVisitorImpl<REALSXP> >{\
    public:                                                                              \
        __CLASS__( const NumericVector& vec_) : PromoteClassVisitor(vec_){}              \
    } ;
    PROMOTE_VISITOR(DateVisitor)
    PROMOTE_VISITOR(POSIXctVisitor)
    
    inline VectorVisitor* visitor( SEXP vec ){
        switch( TYPEOF(vec) ){
            case INTSXP: 
                if( Rf_inherits(vec, "factor" ))
                    return new FactorVisitor( vec ) ;
                return new VectorVisitorImpl<INTSXP>( vec ) ;
            case REALSXP:
                if( Rf_inherits( vec, "Date" ) )
                    return new DateVisitor( vec ) ;
                if( Rf_inherits( vec, "POSIXct" ) )
                    return new POSIXctVisitor( vec ) ;
                return new VectorVisitorImpl<REALSXP>( vec ) ;
            case LGLSXP:  return new VectorVisitorImpl<LGLSXP>( vec ) ;
            case STRSXP:  return new VectorVisitorImpl<STRSXP>( vec ) ;
            default: break ;
        }
        
        // should not happen
        return 0 ;
    }

}

#endif
